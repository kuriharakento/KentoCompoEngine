#include "graphics/postfx/DepthOfFieldRenderer.h"

#include "base/Camera.h"
#include "base/DirectXCommon.h"
#include "base/Logger.h"
#include "base/RenderTexture.h"
#include "graphics/FrameConstantAllocator.h"
#include "graphics/RenderFormats.h"
#include "graphics/deferred/GBuffer.h"
#include "graphics/view/RenderView.h"
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
constexpr uint32_t kBlurSrvCount = 2;
constexpr float kMaxFocusDistance = 500.0f;
constexpr float kMaxFocusRange = 100.0f;
constexpr float kMaxBlurPixels = 32.0f;
constexpr float kDragSpeed = 0.1f;
} // namespace

DepthOfFieldRenderer::~DepthOfFieldRenderer()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->UnregisterDebugUI(this);
	}
#endif
}

void DepthOfFieldRenderer::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height)
{
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	width_ = width;
	height_ = height;

	target_ = std::make_unique<RenderTexture>();
	target_->Initialize(dxCommon_, srvManager_, width, height, kSceneColorFormat, kClearColor);

	FullscreenPassDesc blurDesc;
	blurDesc.pixelShaderPath = L"Resources/shaders/DepthOfField.PS.hlsl";
	blurDesc.rtvFormat = kSceneColorFormat;
	blurDesc.srvCount = kBlurSrvCount;

	FullscreenPassDesc copyDesc;
	copyDesc.pixelShaderPath = L"Resources/shaders/Copy.PS.hlsl";
	copyDesc.rtvFormat = kSceneColorFormat;
	copyDesc.srvCount = 1;

	std::string error;
	if (!blurPass_.Create(dxCommon_, blurDesc, error) || !copyPass_.Create(dxCommon_, copyDesc, error))
	{
		Logger::Log("DepthOfFieldRenderer: 初期化に失敗しました\n" + error + "\n", Logger::LogLevel::Error);
	}
}

void DepthOfFieldRenderer::Resize(uint32_t width, uint32_t height)
{
	width_ = width;
	height_ = height;
	if (target_)
	{
		target_->Resize(width, height);
	}
}

bool DepthOfFieldRenderer::ReloadShaders(std::string& outError)
{
	return blurPass_.ReloadShaders(outError) && copyPass_.ReloadShaders(outError);
}

void DepthOfFieldRenderer::Draw(Camera* camera, RenderView* view, FrameConstantAllocator* allocator)
{
	if (!settings_.enabled || !blurPass_.IsReady() || !copyPass_.IsReady() || !camera || !view || !view->IsValid() || !allocator || !target_)
	{
		return;
	}
	RenderTexture* sceneColor = view->GetSceneColor();
	// サイズが違うビュー（サブビュー）には掛けない
	if (view->GetWidth() != width_ || view->GetHeight() != height_)
	{
		return;
	}

	auto allocation = allocator->Allocate(sizeof(ConstantsForGPU));
	if (!allocation.cpuAddress)
	{
		return;
	}
	ConstantsForGPU constants{};
	constants.invViewProjection = Inverse(camera->GetViewProjectionMatrix());
	constants.cameraPosition = camera->GetTranslate();
	constants.focusDistance = settings_.focusDistance;
	constants.focusRange = settings_.focusRange;
	constants.maxBlurPixels = settings_.maxBlurPixels;
	constants.invTextureSize = { 1.0f / static_cast<float>(width_), 1.0f / static_cast<float>(height_) };
	*static_cast<ConstantsForGPU*>(allocation.cpuAddress) = constants;

	// ぼかした絵を自分のターゲットへ
	target_->BeginRender();
	const D3D12_GPU_DESCRIPTOR_HANDLE blurInputs[kBlurSrvCount] = {
		sceneColor->GetGPUHandle(),
		srvManager_->GetGPUDescriptorHandle(view->GetGBuffer()->GetDepthSRVIndex()),
	};
	blurPass_.Draw(allocation.gpuAddress, blurInputs, kBlurSrvCount);
	target_->EndRender();

	// シーンカラーへ書き戻す。後のポストプロセスはシーンカラーを読むので、流れを変えずに済む
	sceneColor->PreDrawForImGui();
	const D3D12_GPU_DESCRIPTOR_HANDLE copyInput = target_->GetGPUHandle();
	copyPass_.Draw(0, &copyInput, 1);
	sceneColor->EndRender();
}

#ifdef USE_IMGUI
void DepthOfFieldRenderer::RegisterDebugUI()
{
	DebugUIManager::GetInstance()->RegisterDebugUI(this, "Depth Of Field", [this]() { DrawImGui(); }, DebugUIArea::Inspector);
}

void DepthOfFieldRenderer::DrawImGui()
{
	ImGui::Checkbox("Enabled", &settings_.enabled);
	ImGui::DragFloat("Focus Distance", &settings_.focusDistance, kDragSpeed, 0.0f, kMaxFocusDistance, "%.2f");
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("カメラからピントを合わせる所までの距離"); }
	ImGui::DragFloat("Focus Range", &settings_.focusRange, kDragSpeed, 0.0f, kMaxFocusRange, "%.2f");
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("ピントが合って見える奥行きの幅"); }
	ImGui::SliderFloat("Max Blur (px)", &settings_.maxBlurPixels, 0.0f, kMaxBlurPixels, "%.1f");
}
#endif
} // namespace KCE
