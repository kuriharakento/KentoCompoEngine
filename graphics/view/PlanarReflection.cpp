#include "graphics/view/PlanarReflection.h"

#include "base/Camera.h"
#include "base/DirectXCommon.h"
#include "base/Logger.h"
#include "base/RenderTexture.h"
#include "graphics/FrameConstantAllocator.h"
#include "graphics/RenderFormats.h"
#include "graphics/deferred/GBuffer.h"
#include "graphics/view/ISubViewProvider.h"
#include "graphics/view/RenderView.h"
#include "manager/scene/CameraManager.h"
#include "manager/system/SrvManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "manager/editor/DebugUIManager.h"
#endif

namespace KCE
{
namespace
{
constexpr const char* kCameraName = "PlanarReflection";
// 反射はぼやけていて構わないので、画面の半分の解像度で描く
constexpr uint32_t kResolutionDivisor = 2;
constexpr uint32_t kSrvCount = 3;
constexpr float kMaxStrength = 2.0f;
constexpr float kMaxFresnelPower = 16.0f;
constexpr float kMaxHeightTolerance = 1.0f;
constexpr float kDragSpeed = 0.01f;

uint32_t ReducedSize(uint32_t size)
{
	return (size + kResolutionDivisor - 1) / kResolutionDivisor;
}
} // namespace

PlanarReflection::~PlanarReflection()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->UnregisterDebugUI(this);
	}
#endif
}

bool PlanarReflection::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, ISubViewProvider* provider, CameraManager* cameraManager, uint32_t width, uint32_t height)
{
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	provider_ = provider;
	if (!provider_ || !cameraManager)
	{
		return false;
	}

	FullscreenPassDesc desc;
	desc.pixelShaderPath = L"Resources/shaders/Reflection.PS.hlsl";
	desc.rtvFormat = kSceneColorFormat;
	// 足し込むと床が明るくなりすぎる（空が映ると床の照明が消える）ので、反射率の分だけ混ぜる
	desc.blend = FullscreenBlend::Alpha;
	desc.srvCount = kSrvCount;
	std::string error;
	if (!pass_.Create(dxCommon_, desc, error))
	{
		Logger::Log("PlanarReflection: 初期化に失敗しました\n" + error + "\n", Logger::LogLevel::Error);
		return false;
	}

	cameraManager->AddCamera(kCameraName);
	camera_ = cameraManager->GetCamera(kCameraName);
	view_ = provider_->CreateSubView(kCameraName, ReducedSize(width), ReducedSize(height));
	if (!view_ || !camera_)
	{
		return false;
	}
	view_->SetCamera(camera_);
	view_->SetLayerMask(kRenderLayerAll & ~kReflectorLayer);
	// 床に映った輪郭線はほとんど見えないので省く（反射は毎フレーム描くので、少しでも軽くする）
	view_->SetPassEnabled(RenderViewPass::Outline, false);
	// 有効にするまで描かない
	view_->SetEnabled(false);
	return true;
}

void PlanarReflection::Finalize()
{
	if (provider_ && view_)
	{
		provider_->DestroySubView(view_);
	}
	view_ = nullptr;
	provider_ = nullptr;
	camera_ = nullptr;
	hasImage_ = false;
}

void PlanarReflection::Resize(uint32_t width, uint32_t height)
{
	if (view_)
	{
		view_->Resize(ReducedSize(width), ReducedSize(height));
		// 作り直したターゲットは描く前の状態に戻る
		hasImage_ = false;
	}
}

void PlanarReflection::SyncCamera(const Camera* mainCamera)
{
	if (!view_ || !camera_)
	{
		return;
	}
	const bool active = settings_.enabled && mainCamera;
	view_->SetEnabled(active);
	if (!active)
	{
		hasImage_ = false;
		return;
	}

	// Y を床の高さで折り返す鏡映 M = diag(1, -1, 1)。
	// 回転 R を鏡映すると M R M になり、クォータニオンでは x と z の符号が反転する。
	// こうして作ったカメラの像は、元のカメラの像を上下に反転したものと一致する
	Vector3 position = mainCamera->GetTranslate();
	position.y = settings_.planeHeight * 2.0f - position.y;
	Quaternion rotation = mainCamera->GetRotateQuaternion();
	rotation.x = -rotation.x;
	rotation.z = -rotation.z;

	camera_->SetTranslate(position);
	camera_->SetRotateQuaternion(rotation);
	camera_->SetFovY(mainCamera->GetFovY());
	camera_->SetAspectRatio(mainCamera->GetAspectRatio());

	// このあとサブビューとして描かれるので、次のフレームからは読める
	hasImage_ = true;
}

void PlanarReflection::Composite(Camera* camera, GBuffer* gBuffer, D3D12_CPU_DESCRIPTOR_HANDLE sceneColorRtv, FrameConstantAllocator* allocator)
{
	if (!settings_.enabled || !hasImage_ || !pass_.IsReady() || !camera || !gBuffer || !allocator || !view_ || !view_->GetSceneColor())
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
	constants.planeHeight = settings_.planeHeight;
	constants.strength = settings_.strength;
	constants.fresnelPower = settings_.fresnelPower;
	constants.minReflectance = settings_.minReflectance;
	constants.heightTolerance = settings_.heightTolerance;
	*static_cast<ConstantsForGPU*>(allocation.cpuAddress) = constants;

	auto* commandList = dxCommon_->GetCommandList();
	gBuffer->TransitionDepthToSRV();
	commandList->OMSetRenderTargets(1, &sceneColorRtv, FALSE, nullptr);

	const D3D12_GPU_DESCRIPTOR_HANDLE inputs[kSrvCount] = {
		srvManager_->GetGPUDescriptorHandle(gBuffer->GetDepthSRVIndex()),
		srvManager_->GetGPUDescriptorHandle(gBuffer->GetSRVIndex(GBufferIndex::Normal)),
		view_->GetSceneColor()->GetGPUHandle(),
	};
	pass_.Draw(allocation.gpuAddress, inputs, kSrvCount);

	gBuffer->TransitionDepthToDepthWrite();
}

#ifdef USE_IMGUI
void PlanarReflection::RegisterDebugUI()
{
	DebugUIManager::GetInstance()->RegisterDebugUI(this, "Floor Reflection", [this]() { DrawImGui(); }, DebugUIArea::Inspector);
}

void PlanarReflection::DrawImGui()
{
	ImGui::Checkbox("Enabled", &settings_.enabled);
	ImGui::DragFloat("Plane Height", &settings_.planeHeight, kDragSpeed, -100.0f, 100.0f, "%.3f");
	ImGui::SliderFloat("Strength", &settings_.strength, 0.0f, kMaxStrength, "%.2f");
	ImGui::SliderFloat("Fresnel Power", &settings_.fresnelPower, 0.0f, kMaxFresnelPower, "%.1f");
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("浅い角度ほど強く映る度合い"); }
	ImGui::SliderFloat("Min Reflectance", &settings_.minReflectance, 0.0f, 1.0f, "%.2f");
	ImGui::SliderFloat("Height Tolerance", &settings_.heightTolerance, 0.0f, kMaxHeightTolerance, "%.3f");
	ImGui::TextDisabled("反射させる床は PlanarReflection::kReflectorLayer に入れる。");
}
#endif
} // namespace KCE
