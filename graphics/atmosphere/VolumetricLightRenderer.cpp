#include "graphics/atmosphere/VolumetricLightRenderer.h"

#include "base/Camera.h"
#include "base/DirectXCommon.h"
#include "base/Logger.h"
#include "base/RenderTexture.h"
#include "graphics/FrameConstantAllocator.h"
#include "graphics/RenderFormats.h"
#include "graphics/atmosphere/BeamRenderer.h"
#include "graphics/deferred/GBuffer.h"
#include "manager/graphics/ShadowMapManager.h"
#include "manager/scene/LightManager.h"
#include "manager/system/SrvManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "manager/editor/DebugUIManager.h"
#endif

namespace KCE
{
namespace
{
constexpr Vector4 kClearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
constexpr uint32_t kResolutionDivisor = 2;
// t0: 深度、t1〜: ライトごとのシャドウマップ
constexpr uint32_t kMarchSrvCount = 1 + VolumetricLightRenderer::kMaxLights;
// t0: 深度、t1: 半分の解像度の光の筋
constexpr uint32_t kCompositeSrvCount = 2;
constexpr int32_t kMinSteps = 4;
constexpr int32_t kMaxSteps = 128;
constexpr float kMaxDensity = 0.5f;
constexpr float kMaxAnisotropy = 0.95f;
constexpr float kMaxDistance = 200.0f;
constexpr float kMaxIntensity = 50.0f;

uint32_t ReducedSize(uint32_t size)
{
	return (size + kResolutionDivisor - 1) / kResolutionDivisor;
}
} // namespace

VolumetricLightRenderer::~VolumetricLightRenderer()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->Unregister(this);
	}
#endif
}

void VolumetricLightRenderer::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height)
{
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	halfWidth_ = ReducedSize(width);
	halfHeight_ = ReducedSize(height);

	halfTarget_ = std::make_unique<RenderTexture>();
	halfTarget_->Initialize(dxCommon_, srvManager_, halfWidth_, halfHeight_, kSceneColorFormat, kClearColor);

	FullscreenPassDesc marchDesc;
	marchDesc.pixelShaderPath = L"Resources/shaders/VolumetricMarch.PS.hlsl";
	marchDesc.rtvFormat = kSceneColorFormat;
	marchDesc.srvCount = kMarchSrvCount;

	FullscreenPassDesc compositeDesc;
	compositeDesc.pixelShaderPath = L"Resources/shaders/VolumetricComposite.PS.hlsl";
	compositeDesc.rtvFormat = kSceneColorFormat;
	compositeDesc.blend = FullscreenBlend::Additive;
	compositeDesc.srvCount = kCompositeSrvCount;

	std::string error;
	if (!marchPass_.Create(dxCommon_, marchDesc, error) || !compositePass_.Create(dxCommon_, compositeDesc, error))
	{
		Logger::Log("VolumetricLightRenderer: 初期化に失敗しました\n" + error + "\n", Logger::LogLevel::Error);
	}
}

void VolumetricLightRenderer::Resize(uint32_t width, uint32_t height)
{
	halfWidth_ = ReducedSize(width);
	halfHeight_ = ReducedSize(height);
	if (halfTarget_)
	{
		halfTarget_->Resize(halfWidth_, halfHeight_);
	}
}

bool VolumetricLightRenderer::ReloadShaders(std::string& outError)
{
	return marchPass_.ReloadShaders(outError) && compositePass_.ReloadShaders(outError);
}

void VolumetricLightRenderer::Draw(Camera* camera, GBuffer* gBuffer, D3D12_CPU_DESCRIPTOR_HANDLE sceneColorRtv, uint32_t viewWidth, uint32_t viewHeight,
	LightManager* lightManager, ShadowMapManager* shadowMapManager, BeamRenderer* beamRenderer, FrameConstantAllocator* allocator)
{
	if (!settings_.enabled || !marchPass_.IsReady() || !compositePass_.IsReady() || !camera || !gBuffer || !lightManager || !allocator || !halfTarget_
		|| viewWidth == 0 || viewHeight == 0)
	{
		return;
	}
	// 半分のターゲットは画面の大きさで作っているので、それと違うビュー（サブビュー）には掛けない
	if (ReducedSize(viewWidth) != halfWidth_ || ReducedSize(viewHeight) != halfHeight_)
	{
		return;
	}

	auto allocation = allocator->Allocate(sizeof(ConstantsForGPU));
	if (!allocation.cpuAddress)
	{
		return;
	}

	const D3D12_GPU_DESCRIPTOR_HANDLE depthSrv = srvManager_->GetGPUDescriptorHandle(gBuffer->GetDepthSRVIndex());
	D3D12_GPU_DESCRIPTOR_HANDLE marchInputs[kMarchSrvCount];
	marchInputs[0] = depthSrv;

	ConstantsForGPU constants{};
	constants.invViewProjection = Inverse(camera->GetViewProjectionMatrix());
	constants.cameraPosition = camera->GetTranslate();
	constants.density = settings_.density;
	constants.anisotropy = settings_.anisotropy;
	constants.maxDistance = settings_.maxDistance;
	constants.stepCount = settings_.stepCount;
	constants.invFullSize = { 1.0f / static_cast<float>(viewWidth), 1.0f / static_cast<float>(viewHeight) };
	constants.invHalfSize = { 1.0f / static_cast<float>(halfWidth_), 1.0f / static_cast<float>(halfHeight_) };

	// ビームを出しているライトだけを照らす。ビームの明るさの倍率もそのまま使う
	uint32_t lightCount = 0;
	for (const auto& [name, light] : lightManager->GetSpotLights())
	{
		if (lightCount >= kMaxLights)
		{
			break;
		}
		float scale = settings_.intensity * light.gpuData.intensity;
		if (beamRenderer)
		{
			if (!beamRenderer->IsBeamEnabled(name))
			{
				continue;
			}
			scale *= beamRenderer->GetBeamScale(name);
		}
		if (scale <= 0.0f)
		{
			continue;
		}

		LightForGPU& gpu = constants.lights[lightCount];
		gpu.position = light.gpuData.position;
		gpu.range = light.gpuData.distance;
		gpu.direction = light.gpuData.direction;
		gpu.cosAngle = light.gpuData.cosAngle;
		gpu.color = { light.gpuData.color.x * scale, light.gpuData.color.y * scale, light.gpuData.color.z * scale };
		gpu.cosFalloffStart = light.gpuData.cosFalloffStart;
		gpu.decay = light.gpuData.decay;

		const bool hasShadow = light.shadowEnabled && shadowMapManager && shadowMapManager->HasSpotLightShadowMap(name);
		gpu.shadowEnabled = hasShadow ? 1 : 0;
		gpu.shadowViewProj = light.viewProjectionMatrix;
		// 影の無いライトの枠には、型の同じ深度を仮に差しておく（シェーダーは読まない）
		marchInputs[1 + lightCount] = hasShadow ? srvManager_->GetGPUDescriptorHandle(shadowMapManager->GetSpotLightShadowMap(name).srvIndex) : depthSrv;
		++lightCount;
	}
	if (lightCount == 0)
	{
		return;
	}
	for (uint32_t i = lightCount; i < kMaxLights; ++i)
	{
		marchInputs[1 + i] = depthSrv;
	}
	constants.lightCount = static_cast<int32_t>(lightCount);
	*static_cast<ConstantsForGPU*>(allocation.cpuAddress) = constants;

	auto* commandList = dxCommon_->GetCommandList();
	gBuffer->TransitionDepthToSRV();

	// 半分の解像度で光線を進める
	halfTarget_->BeginRender();
	marchPass_.Draw(allocation.gpuAddress, marchInputs, kMarchSrvCount);
	halfTarget_->EndRender();

	// シーンへ戻して、深度を見ながら拡大して足す
	commandList->OMSetRenderTargets(1, &sceneColorRtv, FALSE, nullptr);
	D3D12_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(viewWidth), static_cast<float>(viewHeight), 0.0f, 1.0f };
	D3D12_RECT scissor{ 0, 0, static_cast<LONG>(viewWidth), static_cast<LONG>(viewHeight) };
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissor);
	const D3D12_GPU_DESCRIPTOR_HANDLE compositeInputs[kCompositeSrvCount] = { depthSrv, halfTarget_->GetGPUHandle() };
	compositePass_.Draw(allocation.gpuAddress, compositeInputs, kCompositeSrvCount);

	gBuffer->TransitionDepthToDepthWrite();
}

#ifdef USE_IMGUI
void VolumetricLightRenderer::RegisterDebugUI()
{
	DebugUIManager::GetInstance()->RegisterSettingsPage(this, "Rendering", "Volumetric Light", [this]() { DrawImGui(); });
}

void VolumetricLightRenderer::DrawImGui()
{
	ImGui::Checkbox("Enabled", &settings_.enabled);
	ImGui::SliderFloat("Density", &settings_.density, 0.0f, kMaxDensity, "%.3f");
	ImGui::SliderFloat("Anisotropy", &settings_.anisotropy, -kMaxAnisotropy, kMaxAnisotropy, "%.2f");
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("正にするとライトの方を向いたときに明るく見える"); }
	ImGui::SliderInt("Steps", &settings_.stepCount, kMinSteps, kMaxSteps);
	ImGui::SliderFloat("Max Distance", &settings_.maxDistance, 1.0f, kMaxDistance, "%.1f");
	ImGui::SliderFloat("Intensity", &settings_.intensity, 0.0f, kMaxIntensity, "%.2f");
	ImGui::TextDisabled("照らすのはビームを出しているライトの先頭 %u 本。", kMaxLights);
}
#endif
} // namespace KCE
