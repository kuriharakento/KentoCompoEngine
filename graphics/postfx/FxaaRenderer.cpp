#include "graphics/postfx/FxaaRenderer.h"

#include "base/DirectXCommon.h"
#include "base/Logger.h"
#include "base/RenderTexture.h"
#include "graphics/FrameConstantAllocator.h"
#include "graphics/RenderFormats.h"
#include "manager/system/SrvManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "manager/editor/DebugUIManager.h"
#endif

namespace KCE
{
namespace
{
constexpr Vector4 kClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
constexpr float kMinSubpixel = 0.0f;
constexpr float kMaxSubpixel = 1.0f;
constexpr float kMinEdgeThreshold = 0.03f;
constexpr float kMaxEdgeThreshold = 0.5f;
} // namespace

FxaaRenderer::~FxaaRenderer()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->UnregisterDebugUI(this);
	}
#endif
}

void FxaaRenderer::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height)
{
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	width_ = width;
	height_ = height;

	// ポストプロセスのパイプラインは最終出力のフォーマットで作られているので、それに合わせる
	inputTarget_ = std::make_unique<RenderTexture>();
	inputTarget_->Initialize(dxCommon_, srvManager_, width, height, kDisplayColorFormat, kClearColor);

	FullscreenPassDesc desc;
	desc.pixelShaderPath = L"Resources/shaders/Fxaa.PS.hlsl";
	desc.rtvFormat = kDisplayColorFormat;
	desc.srvCount = 1;
	std::string error;
	if (!pass_.Create(dxCommon_, desc, error))
	{
		Logger::Log("FxaaRenderer: 初期化に失敗しました\n" + error + "\n", Logger::LogLevel::Error);
	}
}

void FxaaRenderer::Resize(uint32_t width, uint32_t height)
{
	width_ = width;
	height_ = height;
	if (inputTarget_)
	{
		inputTarget_->Resize(width, height);
	}
}

bool FxaaRenderer::IsActive() const
{
	return settings_.enabled && pass_.IsReady() && inputTarget_;
}

void FxaaRenderer::Apply(RenderTexture* output, FrameConstantAllocator* allocator)
{
	if (!IsActive() || !allocator)
	{
		return;
	}
	auto allocation = allocator->Allocate(sizeof(ConstantsForGPU));
	if (!allocation.cpuAddress)
	{
		return;
	}

	const float width = static_cast<float>(width_);
	const float height = static_cast<float>(height_);
	ConstantsForGPU constants{};
	constants.invTextureSize = { 1.0f / width, 1.0f / height };
	constants.subpixel = settings_.subpixel;
	constants.edgeThreshold = settings_.edgeThreshold;
	constants.edgeThresholdMin = settings_.edgeThresholdMin;
	*static_cast<ConstantsForGPU*>(allocation.cpuAddress) = constants;

	auto* commandList = dxCommon_->GetCommandList();
	if (output)
	{
		output->BeginRender();
	}
	else
	{
		// バックバッファは BackBufferPreparePass で描画可能になっている
		const UINT index = dxCommon_->GetCurrentBackBufferIndex();
		D3D12_CPU_DESCRIPTOR_HANDLE rtv = dxCommon_->GetCPUDescriptorHandle(dxCommon_->GetRTVDescriptorHeap(), dxCommon_->GetDescriptorSizeRTV(), index);
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = dxCommon_->GetDSVHandle();
		commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

		D3D12_VIEWPORT viewport{ 0.0f, 0.0f, width, height, 0.0f, 1.0f };
		D3D12_RECT scissor{ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &scissor);
	}

	const D3D12_GPU_DESCRIPTOR_HANDLE input = inputTarget_->GetGPUHandle();
	pass_.Draw(allocation.gpuAddress, &input, 1);

	if (output)
	{
		output->EndRender();
	}
}

#ifdef USE_IMGUI
void FxaaRenderer::RegisterDebugUI()
{
	DebugUIManager::GetInstance()->RegisterDebugUI(this, "Anti-Aliasing", [this]() { DrawImGui(); }, DebugUIArea::Inspector);
}

void FxaaRenderer::DrawImGui()
{
	ImGui::Checkbox("FXAA", &settings_.enabled);
	ImGui::SliderFloat("Subpixel", &settings_.subpixel, kMinSubpixel, kMaxSubpixel, "%.2f");
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("1ピクセルより細かい段差をどれだけ均すか。上げるほどぼける"); }
	ImGui::SliderFloat("Edge Threshold", &settings_.edgeThreshold, kMinEdgeThreshold, kMaxEdgeThreshold, "%.3f");
	ImGui::SliderFloat("Edge Threshold Min", &settings_.edgeThresholdMin, 0.0f, kMaxEdgeThreshold, "%.3f");
}
#endif
} // namespace KCE
