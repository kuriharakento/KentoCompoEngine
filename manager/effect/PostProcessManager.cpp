#include "PostProcessManager.h"

#include "DirectXTex/d3dx12.h"
#include <cassert>
// system
#include "engine/base/DirectXCommon.h"
#include "manager/system/SrvManager.h"
#include "base/RenderTexture.h"
#include "base/Logger.h"

#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <Windows.h>
#include <DirectXPackedVector.h>

namespace KCE
{
PostProcessManager::PostProcessManager() {}

PostProcessManager::~PostProcessManager() {}

void PostProcessManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const std::wstring& vsPath, const std::wstring& psPath, uint32_t width, uint32_t height)
{
	// 引数をメンバ変数に記録
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	// パイプライン作成
	SetupPipeline(vsPath, psPath);
	// Bloom用のパイプライン作成
	CreateBloomPipelines();
	// 定数バッファの作成
	CreateConstantBuffer();

	// 各エフェクトの初期化
	grayscaleEffect_ = std::make_unique<GrayscaleEffect>();
	vignetteEffect_ = std::make_unique<VignetteEffect>();
	noiseEffect_ = std::make_unique<NoiseEffect>();
	crtEffect_ = std::make_unique<CRTEffect>();
	bloomEffect_ = std::make_unique<BloomEffect>();

	// ブルームの初期テクセルサイズを設定
	bloomEffect_->SetInvScreenSize({ 1.0f / width, 1.0f / height });

	// 前フレームパラメータを初期化
	preParams_ = {};

	// ビューポートの設定
	viewport_.Width = static_cast<float>(width);
	viewport_.Height = static_cast<float>(height);
	viewport_.TopLeftX = 0;
	viewport_.TopLeftY = 0;
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;

	// シザー矩形の設定
	scissorRect_.left = 0;
	scissorRect_.right = static_cast<LONG>(width);
	scissorRect_.top = 0;
	scissorRect_.bottom = static_cast<LONG>(height);

	InitializeBloomGpuTiming();
}

void PostProcessManager::InitializeBloomGpuTiming()
{
	if (!dxCommon_ || !dxCommon_->GetTimestampFrequency(bloomTimestampFrequency_))
	{
		Logger::Log("Bloom GPU timestamp is not supported by the direct queue.\n", Logger::LogLevel::Warning);
		return;
	}

	D3D12_QUERY_HEAP_DESC queryDesc{};
	queryDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	queryDesc.Count = kBloomTimestampCount;
	HRESULT hr = dxCommon_->GetDevice()->CreateQueryHeap(
		&queryDesc, IID_PPV_ARGS(&bloomTimestampQueryHeap_));
	if (FAILED(hr))
	{
		bloomTimestampFrequency_ = 0;
		Logger::Log("Failed to create Bloom timestamp query heap.\n", Logger::LogLevel::Warning);
		return;
	}

	const D3D12_HEAP_PROPERTIES readbackHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
	const D3D12_RESOURCE_DESC readbackDesc = CD3DX12_RESOURCE_DESC::Buffer(
		static_cast<UINT64>(sizeof(uint64_t) * kBloomTimestampCount));
	hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&readbackHeap, D3D12_HEAP_FLAG_NONE, &readbackDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
		IID_PPV_ARGS(&bloomTimestampReadback_));
	if (FAILED(hr))
	{
		bloomTimestampQueryHeap_.Reset();
		bloomTimestampFrequency_ = 0;
		Logger::Log("Failed to create Bloom timestamp readback buffer.\n", Logger::LogLevel::Warning);
		return;
	}

	char timingTest[8] = {};
	const DWORD length = GetEnvironmentVariableA(
		"KCE_BLOOM_GPU_TIMING_TEST", timingTest, static_cast<DWORD>(sizeof(timingTest)));
	bloomGpuTimingTestEnabled_ = length > 0 && length < sizeof(timingTest) &&
		!(length == 1 && timingTest[0] == '0');
}

void PostProcessManager::WriteTimestamp(uint32_t queryIndex)
{
	if (!bloomGpuTimingActive_ || !bloomTimestampQueryHeap_ || queryIndex >= kBloomTimestampCount)
	{
		return;
	}
	dxCommon_->GetCommandList()->EndQuery(
		bloomTimestampQueryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, queryIndex);
}

void PostProcessManager::CollectBloomGpuTimings()
{
	if (!bloomTimestampResolvePending_ || !bloomTimestampReadback_ || bloomTimestampFrequency_ == 0)
	{
		return;
	}

	std::array<uint64_t, kBloomTimestampCount> timestamps{};
	const D3D12_RANGE readRange{ 0, sizeof(timestamps) };
	void* mapped = nullptr;
	if (SUCCEEDED(bloomTimestampReadback_->Map(0, &readRange, &mapped)) && mapped)
	{
		std::memcpy(timestamps.data(), mapped, sizeof(timestamps));
		const D3D12_RANGE writtenRange{ 0, 0 };
		bloomTimestampReadback_->Unmap(0, &writtenRange);

		const double tickToMs = 1000.0 / static_cast<double>(bloomTimestampFrequency_);
		auto elapsed = [&](uint32_t begin, uint32_t end)
		{
			return timestamps[end] >= timestamps[begin]
				? static_cast<double>(timestamps[end] - timestamps[begin]) * tickToMs : 0.0;
		};
		bloomGpuTimings_.maskMs = elapsed(0, 1);
		bloomGpuTimings_.sourceExtractionMs = bloomTimestampResolvedEnabled_ ? elapsed(2, 3) : 0.0;
		bloomGpuTimings_.blurHorizontalMs = bloomTimestampResolvedEnabled_ ? elapsed(4, 5) : 0.0;
		bloomGpuTimings_.blurVerticalMs = bloomTimestampResolvedEnabled_ ? elapsed(6, 7) : 0.0;
		bloomGpuTimings_.compositeMs = elapsed(8, 9);
		bloomGpuTimings_.valid = true;

		++bloomGpuTimingObservedFrameCount_;
		constexpr uint64_t kWarmupFrameCount = 60;
		if (bloomGpuTimingObservedFrameCount_ > kWarmupFrameCount)
		{
			++bloomGpuTimingSampleCount_;
			bloomGpuTimingMaskSumMs_ += bloomGpuTimings_.maskMs;
			bloomGpuTimingSourceExtractionSumMs_ += bloomGpuTimings_.sourceExtractionMs;
			bloomGpuTimingBlurHorizontalSumMs_ += bloomGpuTimings_.blurHorizontalMs;
			bloomGpuTimingBlurVerticalSumMs_ += bloomGpuTimings_.blurVerticalMs;
			bloomGpuTimingCompositeSumMs_ += bloomGpuTimings_.compositeMs;
			constexpr uint64_t kReportSampleCount = 300;
			if (bloomGpuTimingTestEnabled_ && bloomGpuTimingSampleCount_ % kReportSampleCount == 0)
			{
				WriteBloomGpuTimingReport();
			}
		}
	}
	bloomTimestampResolvePending_ = false;
}

void PostProcessManager::WriteBloomGpuTimingReport() const
{
	if (bloomGpuTimingSampleCount_ == 0)
	{
		return;
	}
	std::filesystem::create_directories("./application/Resources/test-results/particles");
	const double divisor = static_cast<double>(bloomGpuTimingSampleCount_);
	auto writeReport = [&](const std::string& path)
	{
		std::ofstream report(path, std::ios::trunc);
		report << "status=PASS\n";
		report << "bloom_enabled=" << (bloomTimestampResolvedEnabled_ ? 1 : 0) << '\n';
		report << "bloom_mode=" << (!bloomTimestampResolvedEnabled_ ? "off" :
			(bloomTimestampResolvedSelective_ ? "selective" : "legacy")) << '\n';
		report << "samples=" << bloomGpuTimingSampleCount_ << '\n';
		report << "warmup_frames_excluded=60\n";
		report << "timestamp_frequency=" << bloomTimestampFrequency_ << '\n';
		report << "resolution=" << static_cast<uint32_t>(viewport_.Width) << 'x'
			<< static_cast<uint32_t>(viewport_.Height) << '\n';
		report << "vsync=" << (dxCommon_->IsVSyncEnabled() ? 1 : 0) << '\n';
		report << "adapter=" << dxCommon_->GetAdapterName() << '\n';
		report << "driver_version=" << dxCommon_->GetDriverVersion() << '\n';
		report << "mask_scene_avg_ms=" << bloomGpuTimingMaskSumMs_ / divisor << '\n';
		report << "source_extraction_avg_ms=" << bloomGpuTimingSourceExtractionSumMs_ / divisor << '\n';
		report << "blur_horizontal_avg_ms=" << bloomGpuTimingBlurHorizontalSumMs_ / divisor << '\n';
		report << "blur_vertical_avg_ms=" << bloomGpuTimingBlurVerticalSumMs_ / divisor << '\n';
		report << "composite_avg_ms=" << bloomGpuTimingCompositeSumMs_ / divisor << '\n';
		report << "measured_total_avg_ms=" <<
			(bloomGpuTimingMaskSumMs_ + bloomGpuTimingSourceExtractionSumMs_ + bloomGpuTimingBlurHorizontalSumMs_ +
			 bloomGpuTimingBlurVerticalSumMs_ + bloomGpuTimingCompositeSumMs_) / divisor << '\n';
	};
	writeReport("./application/Resources/test-results/particles/selective_bloom_gpu_timing.txt");
	if (!bloomTimestampResolvedEnabled_)
	{
		writeReport("./application/Resources/test-results/particles/selective_bloom_gpu_timing_off.txt");
	}
	else if (bloomTimestampResolvedSelective_)
	{
		writeReport("./application/Resources/test-results/particles/selective_bloom_gpu_timing_on.txt");
		writeReport("./application/Resources/test-results/particles/selective_bloom_gpu_timing_selective.txt");
	}
	else
	{
		writeReport("./application/Resources/test-results/particles/selective_bloom_gpu_timing_legacy.txt");
	}
}

void PostProcessManager::BeginBloomGpuFrame(bool bloomEnabled, bool selectiveBloomEnabled)
{
	// DirectXCommon::PostDrawは前フレームのFenceを待つため、この時点で前回の
	// ResolveQueryDataをCPUから安全に参照できる。
	CollectBloomGpuTimings();
	CollectBloomMaskDiagnostic();
	bloomGpuTimingActive_ = bloomTimestampQueryHeap_ && bloomTimestampReadback_;
	bloomGpuFrameEnabled_ = bloomEnabled;
	bloomGpuFrameSelective_ = selectiveBloomEnabled;
	bloomMaskGpuScopeEnded_ = false;
	if (!bloomGpuTimingActive_)
	{
		bloomGpuTimings_ = {};
		return;
	}
	WriteTimestamp(0);
}

void PostProcessManager::RequestBloomMaskDiagnostic(uint64_t requestId)
{
	bloomMaskDiagnosticRequestedId_ = requestId;
	bloomMaskDiagnosticRequested_ = true;
}

void PostProcessManager::CaptureRequestedBloomMask(RenderTexture* bloomMask)
{
	if (!bloomMaskDiagnosticRequested_ || !bloomMask || !bloomMask->GetResource())
	{
		return;
	}
	const D3D12_RESOURCE_DESC sourceDesc = bloomMask->GetResource()->GetDesc();
	if (sourceDesc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT)
	{
		bloomMaskDiagnosticRequested_ = false;
		bloomMaskDiagnostic_ = { bloomMaskDiagnosticRequestedId_, 0.0, 0.0, 0, false, true };
		return;
	}

	UINT64 requiredSize = 0;
	dxCommon_->GetDevice()->GetCopyableFootprints(
		&sourceDesc, 0, 1, 0, &bloomMaskDiagnosticFootprint_, nullptr, nullptr, &requiredSize);
	if (!bloomMaskDiagnosticReadback_ || bloomMaskDiagnosticBufferSize_ < requiredSize)
	{
		const D3D12_HEAP_PROPERTIES heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
		const D3D12_RESOURCE_DESC buffer = CD3DX12_RESOURCE_DESC::Buffer(requiredSize);
		if (FAILED(dxCommon_->GetDevice()->CreateCommittedResource(
			&heap, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr, IID_PPV_ARGS(&bloomMaskDiagnosticReadback_))))
		{
			bloomMaskDiagnosticRequested_ = false;
			return;
		}
		bloomMaskDiagnosticBufferSize_ = requiredSize;
	}

	bloomMask->TransitionTo(D3D12_RESOURCE_STATE_COPY_SOURCE);
	D3D12_TEXTURE_COPY_LOCATION destination{};
	destination.pResource = bloomMaskDiagnosticReadback_.Get();
	destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	destination.PlacedFootprint = bloomMaskDiagnosticFootprint_;
	D3D12_TEXTURE_COPY_LOCATION source{};
	source.pResource = bloomMask->GetResource();
	source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	source.SubresourceIndex = 0;
	dxCommon_->GetCommandList()->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
	bloomMask->TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	bloomMaskDiagnosticPendingId_ = bloomMaskDiagnosticRequestedId_;
	bloomMaskDiagnosticPending_ = true;
	bloomMaskDiagnosticRequested_ = false;
}

void PostProcessManager::CollectBloomMaskDiagnostic()
{
	if (!bloomMaskDiagnosticPending_ || !bloomMaskDiagnosticReadback_)
	{
		return;
	}
	const uint32_t width = bloomMaskDiagnosticFootprint_.Footprint.Width;
	const uint32_t height = bloomMaskDiagnosticFootprint_.Footprint.Height;
	const uint32_t rowPitch = bloomMaskDiagnosticFootprint_.Footprint.RowPitch;
	const D3D12_RANGE readRange{ 0, static_cast<SIZE_T>(bloomMaskDiagnosticBufferSize_) };
	void* mapped = nullptr;
	BloomMaskDiagnostic result{};
	result.requestId = bloomMaskDiagnosticPendingId_;
	result.finite = true;
	if (SUCCEEDED(bloomMaskDiagnosticReadback_->Map(0, &readRange, &mapped)) && mapped)
	{
		double sum = 0.0;
		for (uint32_t y = 0; y < height; ++y)
		{
			const auto* row = reinterpret_cast<const uint16_t*>(
				static_cast<const uint8_t*>(mapped) + static_cast<size_t>(y) * rowPitch);
			for (uint32_t x = 0; x < width; ++x)
			{
				const float r = DirectX::PackedVector::XMConvertHalfToFloat(row[x * 4 + 0]);
				const float g = DirectX::PackedVector::XMConvertHalfToFloat(row[x * 4 + 1]);
				const float b = DirectX::PackedVector::XMConvertHalfToFloat(row[x * 4 + 2]);
				const float a = DirectX::PackedVector::XMConvertHalfToFloat(row[x * 4 + 3]);
				if (!std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b) || !std::isfinite(a))
				{
					result.finite = false;
					continue;
				}
				const double energy = static_cast<double>(r) + g + b;
				sum += energy;
				result.maxRgbEnergy = (std::max)(result.maxRgbEnergy, energy);
				if (energy > 1.0e-6) ++result.nonZeroPixels;
			}
		}
		result.averageRgbEnergy = sum / static_cast<double>(static_cast<uint64_t>(width) * height);
		const D3D12_RANGE writtenRange{ 0, 0 };
		bloomMaskDiagnosticReadback_->Unmap(0, &writtenRange);
		result.valid = true;
	}
	bloomMaskDiagnostic_ = result;
	bloomMaskDiagnosticPending_ = false;
}

void PostProcessManager::EndBloomMaskGpuScope()
{
	if (!bloomGpuTimingActive_ || bloomMaskGpuScopeEnded_)
	{
		return;
	}
	WriteTimestamp(1);
	bloomMaskGpuScopeEnded_ = true;
}

void PostProcessManager::ResolveBloomGpuTimestamps()
{
	if (!bloomGpuTimingActive_ || !bloomMaskGpuScopeEnded_)
	{
		return;
	}
	dxCommon_->GetCommandList()->ResolveQueryData(
		bloomTimestampQueryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
		0, kBloomTimestampCount, bloomTimestampReadback_.Get(), 0);
	bloomTimestampResolvePending_ = true;
	bloomTimestampResolvedEnabled_ = bloomGpuFrameEnabled_;
	bloomTimestampResolvedSelective_ = bloomGpuFrameSelective_;
	bloomGpuTimingActive_ = false;
}

void PostProcessManager::SetupPipeline(const std::wstring& vsPath, const std::wstring& psPath)
{
	/*--------------[ ルートシグネチャの作成 ]-----------------*/

	// シーンテクスチャ用ディスクリプタレンジ（t0）
	CD3DX12_DESCRIPTOR_RANGE sceneTextureRange{};
	sceneTextureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	// ブルームテクスチャ用ディスクリプタレンジ（t1）
	CD3DX12_DESCRIPTOR_RANGE bloomTextureRange{};
	bloomTextureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

	// サンプラー用ディスクリプタレンジ
	CD3DX12_DESCRIPTOR_RANGE samplerRange{};
	samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

	// ルートパラメータの設定
	CD3DX12_ROOT_PARAMETER rootParams[4]{};
	rootParams[0].InitAsDescriptorTable(1, &sceneTextureRange, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[1].InitAsDescriptorTable(1, &bloomTextureRange, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[2].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[3].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

	// ルートシグネチャの記述
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc{};
	rootSigDesc.Init(_countof(rootParams), rootParams, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// ルートシグネチャのシリアライズと生成
	Microsoft::WRL::ComPtr<ID3DBlob> sigBlob, errBlob;
	D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
	dxCommon_->GetDevice()->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));

	/*--------------[ パイプラインステートの作成 ]-----------------*/

	// シェーダーのコンパイル
	auto vs = dxCommon_->CompileSharder(vsPath, L"vs_6_0");
	auto ps = dxCommon_->CompileSharder(psPath, L"ps_6_0");

	// パイプラインステートの記述
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	psoDesc.pRootSignature = rootSignature_.Get();
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.NumRenderTargets = 1;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.InputLayout = { nullptr, 0 };
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

	// パイプラインステートの生成
	dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
}

void PostProcessManager::CreateBloomPipelines()
{
	/*--------------[ ルートシグネチャの作成 ]-----------------*/

	// シーンテクスチャ用ディスクリプタレンジ（t0）
	CD3DX12_DESCRIPTOR_RANGE sceneTextureRange{};
	sceneTextureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	// サンプラー用ディスクリプタレンジ
	CD3DX12_DESCRIPTOR_RANGE samplerRange{};
	samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

	// ルートパラメータの設定
	CD3DX12_ROOT_PARAMETER rootParams[3]{};
	rootParams[0].InitAsDescriptorTable(1, &sceneTextureRange, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[1].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[2].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

	// ルートシグネチャの記述
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc{};
	rootSigDesc.Init(_countof(rootParams), rootParams, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// ルートシグネチャのシリアライズと生成
	Microsoft::WRL::ComPtr<ID3DBlob> sigBlob, errBlob;
	D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
	dxCommon_->GetDevice()->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&bloomRootSignature_));


	/*--------------[ ブライトパスパイプラインの作成 ]-----------------*/
	{
		// シェーダーのコンパイル
		auto vs = dxCommon_->CompileSharder(L"Resources/shaders/PostEffect.VS.hlsl", L"vs_6_0");
		auto ps = dxCommon_->CompileSharder(L"Resources/shaders/BrightPass.PS.hlsl", L"ps_6_0");

		// パイプラインステートの記述
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		psoDesc.pRootSignature = bloomRootSignature_.Get();
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;  // HDRフォーマット
		psoDesc.NumRenderTargets = 1;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.InputLayout = { nullptr, 0 };
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

		// パイプラインステートの生成
		dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&brightPassPSO_));
	}

	/*--------------[ ブラーパスパイプラインの作成 ]-----------------*/
	{
		// シェーダーのコンパイル
		auto vs = dxCommon_->CompileSharder(L"Resources/shaders/PostEffect.VS.hlsl", L"vs_6_0");
		auto ps = dxCommon_->CompileSharder(L"Resources/shaders/GaussianBlur.PS.hlsl", L"ps_6_0");

		// パイプラインステートの記述
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		psoDesc.pRootSignature = bloomRootSignature_.Get();
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		psoDesc.NumRenderTargets = 1;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.InputLayout = { nullptr, 0 };
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

		// パイプラインステートの生成
		dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&blurPSO_));
	}

	/*--------------[ 定数バッファの作成 ]-----------------*/

	// バッファサイズを256バイトアラインメント
	size_t brightPassBufferSize = (sizeof(BrightPassParams) + 255) & ~255;
	size_t blurBufferSize = (sizeof(BlurParams) + 255) & ~255;

	// ヒーププロパティの設定
	D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	// ブライトパス用定数バッファの作成
	D3D12_RESOURCE_DESC brightPassBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(brightPassBufferSize);
	dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &brightPassBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&brightPassConstantBuffer_));

	// ブラー用定数バッファの作成
	D3D12_RESOURCE_DESC blurBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(blurBufferSize);
	dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &blurBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&blurConstantBuffer_));
}

void PostProcessManager::RenderBrightPass(RenderTexture* inputTexture, RenderTexture* outputRT)
{
	auto cmdList = dxCommon_->GetCommandList();

	// 出力レンダーターゲットを設定
	outputRT->BeginRender();

	// パイプラインステートとルートシグネチャを設定
	cmdList->SetPipelineState(brightPassPSO_.Get());
	cmdList->SetGraphicsRootSignature(bloomRootSignature_.Get());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ディスクリプタヒープを設定
	ID3D12DescriptorHeap* heaps[] = {
		srvManager_->GetSrvHeap(),
		dxCommon_->GetSamplerHeap()
	};
	cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
	cmdList->SetGraphicsRootDescriptorTable(0, inputTexture->GetGPUHandle());
	cmdList->SetGraphicsRootDescriptorTable(1, dxCommon_->GetSamplerDescriptorHandle());

	// 定数バッファを更新してGPUに転送
	void* mappedData = nullptr;
	brightPassConstantBuffer_->Map(0, nullptr, &mappedData);
	memcpy(mappedData, &brightPassParams_, sizeof(BrightPassParams));
	brightPassConstantBuffer_->Unmap(0, nullptr);

	// 定数バッファビューを設定
	cmdList->SetGraphicsRootConstantBufferView(2, brightPassConstantBuffer_->GetGPUVirtualAddress());

	// フルスクリーン三角形を描画
	cmdList->DrawInstanced(3, 1, 0, 0);

	// レンダーターゲットを終了
	outputRT->EndRender();
}

void PostProcessManager::RenderBlurPass(RenderTexture* inputTexture, RenderTexture* outputRT, bool horizontal)
{
	auto cmdList = dxCommon_->GetCommandList();

	// 出力レンダーターゲットを設定
	outputRT->BeginRender();

	// パイプラインステートとルートシグネチャを設定
	cmdList->SetPipelineState(blurPSO_.Get());
	cmdList->SetGraphicsRootSignature(bloomRootSignature_.Get());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ディスクリプタヒープを設定
	ID3D12DescriptorHeap* heaps[] = {
		srvManager_->GetSrvHeap(),
		dxCommon_->GetSamplerHeap()
	};
	cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
	cmdList->SetGraphicsRootDescriptorTable(0, inputTexture->GetGPUHandle());
	cmdList->SetGraphicsRootDescriptorTable(1, dxCommon_->GetSamplerDescriptorHandle());

	// ブラー方向を設定（水平または垂直）
	blurParams_.texelSize = { 1.0f / viewport_.Width, 1.0f / viewport_.Height };
	blurParams_.blurDirection = horizontal ? Vector2{ 1.0f, 0.0f } : Vector2{ 0.0f, 1.0f };

	// 定数バッファを更新してGPUに転送
	void* mappedData = nullptr;
	blurConstantBuffer_->Map(0, nullptr, &mappedData);
	memcpy(mappedData, &blurParams_, sizeof(BlurParams));
	blurConstantBuffer_->Unmap(0, nullptr);

	// 定数バッファビューを設定
	cmdList->SetGraphicsRootConstantBufferView(2, blurConstantBuffer_->GetGPUVirtualAddress());

	// フルスクリーン三角形を描画
	cmdList->DrawInstanced(3, 1, 0, 0);

	// レンダーターゲットを終了
	outputRT->EndRender();
}

void PostProcessManager::RenderFinalComposite(RenderTexture* sceneTexture, RenderTexture* bloomTexture, RenderTexture* outputRT)
{
	auto cmdList = dxCommon_->GetCommandList();
	auto device = dxCommon_->GetDevice();

	if (outputRT) {
		outputRT->BeginRender();
	}
	else {
		// バックバッファをレンダーターゲットとして設定
		UINT backBufferIndex = dxCommon_->GetCurrentBackBufferIndex();
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dxCommon_->GetCPUDescriptorHandle(dxCommon_->GetRTVDescriptorHeap(), dxCommon_->GetDescriptorSizeRTV(), backBufferIndex);
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVDescriptorHeap()->GetCPUDescriptorHandleForHeapStart();
		cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
	}

	// ビューポートとシザー矩形を設定
	cmdList->RSSetViewports(1, &viewport_);
	cmdList->RSSetScissorRects(1, &scissorRect_);

	// パイプラインステートとルートシグネチャを設定
	cmdList->SetPipelineState(pipelineState_.Get());
	cmdList->SetGraphicsRootSignature(rootSignature_.Get());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ディスクリプタヒープを設定
	ID3D12DescriptorHeap* heaps[] = {
		srvManager_->GetSrvHeap(),
		dxCommon_->GetSamplerHeap()
	};
	cmdList->SetDescriptorHeaps(_countof(heaps), heaps);

	// シーンテクスチャとブルームテクスチャを設定
	cmdList->SetGraphicsRootDescriptorTable(0, sceneTexture->GetGPUHandle());  // t0
	cmdList->SetGraphicsRootDescriptorTable(1, bloomTexture->GetGPUHandle());  // t1
	cmdList->SetGraphicsRootDescriptorTable(2, dxCommon_->GetSamplerDescriptorHandle());

	// 各エフェクトを適用
	grayscaleEffect_->ApplyEffect(params_);
	vignetteEffect_->ApplyEffect(params_);
	noiseEffect_->ApplyEffect(params_);
	crtEffect_->ApplyEffect(params_);
	bloomEffect_->ApplyEffect(params_);

	// 定数バッファを更新
	UpdateConstantBuffer();
	cmdList->SetGraphicsRootConstantBufferView(3, constantBuffer_->GetGPUVirtualAddress());

	// フルスクリーン三角形を描画
	cmdList->DrawInstanced(3, 1, 0, 0);

	if (outputRT) {
		outputRT->EndRender();
	}
}

void PostProcessManager::SetBloomRenderTargets(RenderTexture* brightPassRT, RenderTexture* blurRT0, RenderTexture* blurRT1)
{
	// ブルーム用レンダーターゲットを設定
	brightPassRT_ = brightPassRT;
	blurRT_[0] = blurRT0;
	blurRT_[1] = blurRT1;
}

void PostProcessManager::Resize(uint32_t width, uint32_t height)
{
	// ビューポートとシザー矩形の設定を更新する
	viewport_.Width = static_cast<float>(width);
	viewport_.Height = static_cast<float>(height);
	scissorRect_.right = static_cast<LONG>(width);
	scissorRect_.bottom = static_cast<LONG>(height);

	// ブルームの逆テクセルサイズを更新する
	if (bloomEffect_)
	{
		bloomEffect_->SetInvScreenSize({ 1.0f / width, 1.0f / height });
	}
}


void PostProcessManager::Draw(RenderTexture* inputTexture, RenderTexture* outputRT, RenderTexture* selectiveBloomSource)
{
	// ブルームが有効かつ、必要なレンダーターゲットが存在する場合
	if (bloomEffect_->IsEnabled() && HasBloomRenderTargets())
	{
		// マルチパス・ブルーム処理
		RenderWithBloom(inputTexture, outputRT, selectiveBloomSource);
	}
	else
	{
		// Bloom OFFではBlur区間を空のtimestamp pairとして0にし、
		// Single PassをON時のCompositeと同じ区間番号で比較する。
		WriteTimestamp(2); WriteTimestamp(3);
		WriteTimestamp(4); WriteTimestamp(5);
		WriteTimestamp(6); WriteTimestamp(7);
		WriteTimestamp(8);
		RenderSinglePass(inputTexture, outputRT);
		WriteTimestamp(9);
		ResolveBloomGpuTimestamps();
	}
}

void PostProcessManager::RenderWithBloom(RenderTexture* inputTexture, RenderTexture* outputRT, RenderTexture* selectiveBloomSource)
{
	// ブルームのマルチパス処理

	// 1. Selective source already contains only explicitly emissive drawables.
	// Keep the legacy luminance extraction only as a compatibility fallback.
	RenderTexture* bloomSource = selectiveBloomSource;
	WriteTimestamp(2);
	if (!bloomSource)
	{
		RenderBrightPass(inputTexture, brightPassRT_);
		bloomSource = brightPassRT_;
	}
	WriteTimestamp(3);

	// 2. 水平方向ブラー
	WriteTimestamp(4);
	RenderBlurPass(bloomSource, blurRT_[0], true);
	WriteTimestamp(5);

	// 3. 垂直方向ブラー
	WriteTimestamp(6);
	RenderBlurPass(blurRT_[0], blurRT_[1], false);
	WriteTimestamp(7);

	// 4. 最終合成（シーン + ブルーム）
	WriteTimestamp(8);
	RenderFinalComposite(inputTexture, blurRT_[1], outputRT);
	WriteTimestamp(9);
	ResolveBloomGpuTimestamps();
}

bool PostProcessManager::HasBloomRenderTargets() const
{
	// ブルーム用の全レンダーターゲットが設定されているかチェック
	return brightPassRT_ != nullptr && blurRT_[0] != nullptr && blurRT_[1] != nullptr;
}

void PostProcessManager::RenderSinglePass(RenderTexture* inputTexture, RenderTexture* outputRT)
{
	auto cmdList = dxCommon_->GetCommandList();

	if (outputRT) {
		outputRT->BeginRender();
	}
	else {
		// バックバッファをレンダーターゲットとして設定
		UINT backBufferIndex = dxCommon_->GetCurrentBackBufferIndex();
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dxCommon_->GetCPUDescriptorHandle(dxCommon_->GetRTVDescriptorHeap(), dxCommon_->GetDescriptorSizeRTV(), backBufferIndex);
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVDescriptorHeap()->GetCPUDescriptorHandleForHeapStart();
		cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
	}

	// ビューポートとシザー矩形を設定
	cmdList->RSSetViewports(1, &viewport_);
	cmdList->RSSetScissorRects(1, &scissorRect_);

	// パイプラインステートとルートシグネチャを設定
	cmdList->SetPipelineState(pipelineState_.Get());
	cmdList->SetGraphicsRootSignature(rootSignature_.Get());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ディスクリプタヒープを設定（SRVヒープとサンプラーヒープ）
	ID3D12DescriptorHeap* heaps[] = {
		srvManager_->GetSrvHeap(),
		dxCommon_->GetSamplerHeap()
	};
	cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
	cmdList->SetGraphicsRootDescriptorTable(0, inputTexture->GetGPUHandle());
	cmdList->SetGraphicsRootDescriptorTable(1, blurRT_[1]->GetGPUHandle()); // ダミーのブルームテクスチャ
	cmdList->SetGraphicsRootDescriptorTable(2, dxCommon_->GetSamplerDescriptorHandle());

	// ポストプロセスの各エフェクトを適用
	grayscaleEffect_->ApplyEffect(params_);
	vignetteEffect_->ApplyEffect(params_);
	noiseEffect_->ApplyEffect(params_);
	crtEffect_->ApplyEffect(params_);
	bloomEffect_->ApplyEffect(params_);

	// 定数バッファを更新
	UpdateConstantBuffer();
	// 定数バッファビューを設定
	cmdList->SetGraphicsRootConstantBufferView(3, constantBuffer_->GetGPUVirtualAddress());
	// フルスクリーン三角形を描画
	cmdList->DrawInstanced(3, 1, 0, 0);

	if (outputRT) {
		outputRT->EndRender();
	}
}

void PostProcessManager::CreateConstantBuffer()
{
	// バッファサイズを256バイトアラインメント
	size_t bufferSize = (sizeof(PostEffectParams) + 255) & ~255;

	// ヒーププロパティの設定
	D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

	// 定数バッファの作成
	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constantBuffer_));
	assert(SUCCEEDED(hr));

	// サンプラーヒープの作成
	dxCommon_->CreateSamplerHeap();
}

void PostProcessManager::UpdateConstantBuffer()
{
	// 前フレームと同じパラメータなら更新しない
	if (params_ == preParams_)
	{
		return;
	}

	// 定数バッファにパラメータをコピー
	void* mappedData = nullptr;
	constantBuffer_->Map(0, nullptr, &mappedData);
	memcpy(mappedData, &params_, sizeof(PostEffectParams));
	constantBuffer_->Unmap(0, nullptr);

	// 前フレームのパラメータを更新
	preParams_ = params_;
}
} // namespace KCE
