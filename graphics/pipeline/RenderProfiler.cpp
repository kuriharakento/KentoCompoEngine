#include "graphics/pipeline/RenderProfiler.h"

#include <cstring>

#include "base/DirectXCommon.h"
#include "base/Logger.h"
#include "graphics/pipeline/DrawCallCounter.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "manager/editor/DebugUIManager.h"
#endif

namespace KCE
{
namespace
{
// フレーム全体の始まりと終わりのクエリ番号
constexpr uint32_t kFrameBeginQuery = 0;
constexpr uint32_t kFrameEndQuery = 1;
// 表の数字がちらつかないよう、前の値へ少しずつ寄せる割合（1 で毎回の値そのまま）
constexpr float kSmoothing = 0.1f;
// この回数だけ読んでも出てこなかった行は消す（シーンを切り替えて無くなったビューなど）
constexpr uint64_t kStaleReadCount = 120;
constexpr double kMillisecondsPerSecond = 1000.0;
// 配列に入りきらず測らなかったパスの印
constexpr uint32_t kInvalidSample = UINT32_MAX;
// 表でパス名を字下げする幅（入れ子1段あたりの文字数）
constexpr int kIndentPerDepth = 2;

template <size_t N>
void CopyName(char (&destination)[N], const char* source)
{
	strncpy_s(destination, source ? source : "", _TRUNCATE);
}
} // namespace

RenderProfiler::~RenderProfiler()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->Unregister(this);
	}
#endif
}

void RenderProfiler::Initialize(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;
	if (!dxCommon_)
	{
		return;
	}
	ID3D12Device* device = dxCommon_->GetDevice();

	D3D12_QUERY_HEAP_DESC heapDesc{};
	heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	heapDesc.Count = kMaxQueries;
	if (FAILED(device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&queryHeap_))))
	{
		Logger::Log("RenderProfiler: タイムスタンプのクエリヒープを作れなかったので、GPU 時間は測りません\n", Logger::LogLevel::Warning);
		return;
	}

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_READBACK;
	D3D12_RESOURCE_DESC bufferDesc{};
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Width = sizeof(uint64_t) * kMaxQueries;
	bufferDesc.Height = 1;
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.MipLevels = 1;
	bufferDesc.SampleDesc.Count = 1;
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	// 読み出し用のヒープは COPY_DEST から変えられない
	if (FAILED(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readbackBuffer_))))
	{
		Logger::Log("RenderProfiler: 読み出し用のバッファを作れなかったので、GPU 時間は測りません\n", Logger::LogLevel::Warning);
		queryHeap_.Reset();
		return;
	}

	if (FAILED(dxCommon_->GetCommandQueue()->GetTimestampFrequency(&timestampFrequency_)))
	{
		timestampFrequency_ = 0;
	}
}

bool RenderProfiler::IsReady() const
{
	return dxCommon_ && queryHeap_ && readbackBuffer_ && timestampFrequency_ > 0;
}

void RenderProfiler::BeginFrame()
{
	// 前のフレームの GPU は PostDraw で終わっているので、ここで読んでも待たない
	if (resolved_)
	{
		ReadResults();
		resolved_ = false;
	}

	sampleCount_ = 0;
	openDepth_ = 0;
	skippedDepth_ = 0;
	frameOpen_ = enabled_ && IsReady();
	if (!frameOpen_)
	{
		return;
	}
	frameDrawCallsAtBegin_ = DrawCallCounter::Get();
	dxCommon_->GetCommandList()->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, kFrameBeginQuery);
}

void RenderProfiler::EndFrame()
{
	if (!frameOpen_)
	{
		return;
	}
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, kFrameEndQuery);
	frameDrawCalls_ = static_cast<uint32_t>(DrawCallCounter::Get() - frameDrawCallsAtBegin_);
	commandList->ResolveQueryData(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, kFrameQueryCount + sampleCount_ * 2, readbackBuffer_.Get(), 0);
	resolved_ = true;
	frameOpen_ = false;
}

void RenderProfiler::BeginPass(const char* viewName, const char* passName)
{
	if (!frameOpen_)
	{
		return;
	}
	if (openDepth_ >= kMaxDepth)
	{
		++skippedDepth_;
		return;
	}

	uint32_t index = kInvalidSample;
	if (sampleCount_ < kMaxSamples)
	{
		index = sampleCount_++;
		Sample& sample = samples_[index];
		CopyName(sample.view, viewName);
		CopyName(sample.pass, passName);
		sample.depth = openDepth_;
		sample.drawCallsAtBegin = DrawCallCounter::Get();
		sample.drawCalls = 0;
		sample.ended = false;
		dxCommon_->GetCommandList()->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, kFrameQueryCount + index * 2);
	}
	openStack_[openDepth_++] = index;
}

void RenderProfiler::EndPass()
{
	if (!frameOpen_)
	{
		return;
	}
	if (skippedDepth_ > 0)
	{
		--skippedDepth_;
		return;
	}
	if (openDepth_ == 0)
	{
		return;
	}

	const uint32_t index = openStack_[--openDepth_];
	if (index == kInvalidSample)
	{
		return;
	}
	Sample& sample = samples_[index];
	sample.drawCalls = static_cast<uint32_t>(DrawCallCounter::Get() - sample.drawCallsAtBegin);
	sample.ended = true;
	dxCommon_->GetCommandList()->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, kFrameQueryCount + index * 2 + 1);
}

void RenderProfiler::ReadResults()
{
	const uint32_t queryCount = kFrameQueryCount + sampleCount_ * 2;
	const D3D12_RANGE readRange{ 0, sizeof(uint64_t) * queryCount };
	uint64_t* timestamps = nullptr;
	if (FAILED(readbackBuffer_->Map(0, &readRange, reinterpret_cast<void**>(&timestamps))) || !timestamps)
	{
		return;
	}

	++readCount_;
	const double ticksToMs = kMillisecondsPerSecond / static_cast<double>(timestampFrequency_);
	const auto toMs = [ticksToMs](uint64_t begin, uint64_t end)
	{
		return end >= begin ? static_cast<float>(static_cast<double>(end - begin) * ticksToMs) : 0.0f;
	};

	const float frameMs = toMs(timestamps[kFrameBeginQuery], timestamps[kFrameEndQuery]);
	frameGpuMs_ = (readCount_ > 1) ? frameGpuMs_ + (frameMs - frameGpuMs_) * kSmoothing : frameMs;
	resultFrameDrawCalls_ = frameDrawCalls_;

	// ビュー名＋パス名で行を引き当てて更新する。初めて出てきた行は後ろに足す
	for (uint32_t i = 0; i < sampleCount_; ++i)
	{
		const Sample& sample = samples_[i];
		if (!sample.ended)
		{
			continue;
		}
		const float ms = toMs(timestamps[kFrameQueryCount + i * 2], timestamps[kFrameQueryCount + i * 2 + 1]);

		Result* result = nullptr;
		for (uint32_t r = 0; r < resultCount_; ++r)
		{
			if (std::strcmp(results_[r].view, sample.view) == 0 && std::strcmp(results_[r].pass, sample.pass) == 0)
			{
				result = &results_[r];
				break;
			}
		}
		if (result)
		{
			result->gpuMs += (ms - result->gpuMs) * kSmoothing;
		}
		else
		{
			if (resultCount_ >= kMaxSamples)
			{
				continue;
			}
			result = &results_[resultCount_++];
			CopyName(result->view, sample.view);
			CopyName(result->pass, sample.pass);
			result->gpuMs = ms;
		}
		result->depth = sample.depth;
		result->drawCalls = sample.drawCalls;
		result->lastSeen = readCount_;
	}

	// しばらく出てこない行は消す。並びは保つ
	uint32_t kept = 0;
	for (uint32_t r = 0; r < resultCount_; ++r)
	{
		if (readCount_ - results_[r].lastSeen > kStaleReadCount)
		{
			continue;
		}
		if (kept != r)
		{
			results_[kept] = results_[r];
		}
		++kept;
	}
	resultCount_ = kept;

	// CPU からは書いていないので、書いた範囲は空で返す
	const D3D12_RANGE writtenRange{ 0, 0 };
	readbackBuffer_->Unmap(0, &writtenRange);
}

#ifdef USE_IMGUI
void RenderProfiler::RegisterDebugUI()
{
	DebugUIManager::GetInstance()->RegisterSettingsPage(this, "レンダリング", "描画の計測", [this]() { this->DrawImGui(); });
}

void RenderProfiler::DrawImGui()
{
	ImGui::Checkbox("計測する", &enabled_);
	if (!IsReady())
	{
		ImGui::TextDisabled("この環境ではタイムスタンプを測れません");
		return;
	}

	ImGui::Text("GPU 全体: %.2f ms    ドローコール: %u", frameGpuMs_, resultFrameDrawCalls_);
	ImGui::TextDisabled("ドローコールは 3D の物（モデル・スキンメッシュ・インスタンシング・パーティクルのメッシュ）だけ数える");
	ImGui::TextDisabled("サブビューは描き直したフレームの値（24fps のモニターなど）。SubViews の時間は中のサブビューを含む");

	const ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;
	if (ImGui::BeginTable("##RenderProfiler", 4, tableFlags))
	{
		ImGui::TableSetupColumn("ビュー");
		ImGui::TableSetupColumn("パス");
		ImGui::TableSetupColumn("GPU (ms)");
		ImGui::TableSetupColumn("ドローコール");
		ImGui::TableHeadersRow();
		for (uint32_t r = 0; r < resultCount_; ++r)
		{
			const Result& result = results_[r];
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(result.view);
			ImGui::TableNextColumn();
			ImGui::Text("%*s%s", static_cast<int>(result.depth) * kIndentPerDepth, "", result.pass);
			ImGui::TableNextColumn();
			ImGui::Text("%.3f", result.gpuMs);
			ImGui::TableNextColumn();
			ImGui::Text("%u", result.drawCalls);
		}
		ImGui::EndTable();
	}
}
#endif
} // namespace KCE
