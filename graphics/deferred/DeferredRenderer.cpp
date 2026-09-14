#include "DeferredRenderer.h"
#include "base/DirectXCommon.h"
#include "manager/system/SrvManager.h"
#include "manager/scene/LightManager.h"
#include "manager/graphics/ShadowMapManager.h"
#include "manager/scene/CameraManager.h"
#include "base/Camera.h"
#include "base/Logger.h"
#include "graphics/FrameConstantAllocator.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "gameobject/base/GameObject.h"
#include "gameobject/manager/GameObjectManager.h"
#include "manager/editor/DebugUIManager.h"
#endif
#include <cassert>

namespace KCE
{
void DeferredRenderer::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	assert(dxCommon);
	assert(srvManager);

	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	// G-Buffer はビュー（RenderView）が持つ。
	// 解像度ごとに必要なので共有できないが、パイプラインは共有できる。

	// パイプライン
	gBufferPipeline_ = std::make_unique<GBufferPipeline>();
	gBufferPipeline_->Initialize(dxCommon);

	lightPassPipeline_ = std::make_unique<LightPassPipeline>();
	lightPassPipeline_->Initialize(dxCommon);

	// バッファ作成
	CreateCameraBuffer();
	CreateLightBuffer();

	// トゥーン（NPR）の全体設定
	toonBuffer_ = dxCommon_->CreateBufferResource(sizeof(ToonSettingsForGPU));
	toonBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&toonData_));
	*toonData_ = toonSettings_;

	KCE::Logger::Log("DeferredRenderer initialized\n");
}

DeferredRenderer::~DeferredRenderer()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->Unregister(this);
	}
#endif
}

#ifdef USE_IMGUI
void DeferredRenderer::RegisterDebugUI()
{
	DebugUIManager::GetInstance()->RegisterSettingsPage(this, "レンダリング", "NPRシェーディング", [this]() { DrawImGui(); });
}

void DeferredRenderer::DrawImGui()
{
	ImGui::TextDisabled("トゥーンの効き具合とリムの強さは素材ごと（SetToonAmount / SetRimStrength）");
	ImGui::TextDisabled("ここはディファード描画の全体設定。フォワード描画は既定値を使う");
	ImGui::Separator();

	ImGui::DragFloat("しきい値", &toonSettings_.threshold, 0.005f, 0.0f, 1.0f, "%.3f");
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("明部と暗部の境界（NdotL）"); }
	ImGui::DragFloat("境界のぼかし", &toonSettings_.softness, 0.002f, 0.0f, 0.5f, "%.3f");
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("境界のぼかし幅。0 でくっきり、大きいほど柔らかい"); }
	ImGui::ColorEdit3("影の色味", &toonSettings_.shadowTint.x);
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("暗部に乗算する色。黒で落とすと濁るので、少し色味を残す"); }

	ImGui::Separator();
	ImGui::ColorEdit3("リムの色", &toonSettings_.rimColor.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
	ImGui::DragFloat("リムの絞り", &toonSettings_.rimPower, 0.05f, 0.5f, 16.0f, "%.2f");
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("大きいほど輪郭の細い範囲だけが光る"); }

	if (ImGui::Button("リセット"))
	{
		toonSettings_ = ToonSettingsForGPU{};
	}

	// 素材の既定は効き具合0（従来の見た目）なので、そのままでは違いを確認できない。
	// 見比べるために、シーンの全オブジェクトへまとめて適用できるようにしておく。
	ImGui::SeparatorText("全オブジェクトへ適用（確認用）");
	ImGui::DragFloat("トゥーンの効き", &previewToonAmount_, 0.01f, 0.0f, 1.0f, "%.2f");
	ImGui::DragFloat("リムの強さ", &previewRimStrength_, 0.01f, 0.0f, 1.0f, "%.2f");
	ImGui::DragFloat("輪郭線の強さ", &previewOutlineStrength_, 0.01f, 0.0f, 1.0f, "%.2f");
	if (ImGui::Button("全オブジェクトに適用") && GameObjectManager::HasInstance())
	{
		for (GameObject* object : GameObjectManager::GetInstance()->GetGameObjects())
		{
			if (IRenderable3d* renderable = object ? object->GetRenderable3d() : nullptr)
			{
				renderable->SetToonAmount(previewToonAmount_);
				renderable->SetRimStrength(previewRimStrength_);
				renderable->SetOutlineStrength(previewOutlineStrength_);
			}
		}
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("モデルは共有されうるため、同じモデルを使う他のオブジェクトにも効きます");
	}
}
#endif

void DeferredRenderer::CreateCameraBuffer()
{
	cameraBuffer_ = dxCommon_->CreateBufferResource(sizeof(CameraDataForGPU));
	cameraBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));
}

void DeferredRenderer::CreateLightBuffer()
{
	lightBuffer_ = dxCommon_->CreateBufferResource(sizeof(LightBufferForGPU));
	lightBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&lightBufferData_));

	// 初期化
	if (lightBufferData_)
	{
		memset(lightBufferData_, 0, sizeof(LightBufferForGPU));
	}
}

void DeferredRenderer::BeginGeometryPass(GBuffer* gBuffer)
{
	if (!gBuffer) return;

	gBuffer->BeginGeometryPass();
	gBufferPipeline_->SetPipeline();
}

void DeferredRenderer::EndGeometryPass(GBuffer* gBuffer)
{
	if (!gBuffer) return;

	gBuffer->EndGeometryPass();
}

D3D12_GPU_VIRTUAL_ADDRESS DeferredRenderer::UpdateCameraBuffer(Camera* camera, FrameConstantAllocator* allocator)
{
	if (!camera || !cameraData_) return cameraBuffer_->GetGPUVirtualAddress();

	// サブビューも同じフレームでライトパスを回すので、1本のバッファに書くと
	// GPU が読む前に後のビューのカメラで上書きされる。フレーム用の領域に書き分ける
	CameraDataForGPU* data = cameraData_;
	D3D12_GPU_VIRTUAL_ADDRESS address = cameraBuffer_->GetGPUVirtualAddress();
	if (allocator)
	{
		auto allocation = allocator->Allocate(sizeof(CameraDataForGPU));
		if (allocation.cpuAddress)
		{
			data = static_cast<CameraDataForGPU*>(allocation.cpuAddress);
			address = allocation.gpuAddress;
		}
	}

	data->worldPos = camera->GetTranslate();
	data->padding0 = 0.0f;
	data->viewMatrix = camera->GetViewMatrix();
	data->projMatrix = camera->GetProjectionMatrix();
	data->invViewMatrix = Inverse(data->viewMatrix);
	data->invProjMatrix = Inverse(data->projMatrix);
	data->nearPlane = 0.1f;
	data->farPlane = 200.0f;
	return address;
}

void DeferredRenderer::UpdateLightBuffer(LightManager* lightManager, ShadowMapManager* shadowMapManager)
{
	if (!lightManager || !lightBufferData_) return;

	auto& spotLights = lightManager->GetSpotLights();
	auto& pointLights = lightManager->GetPointLights();

	// スポットライト
	lightBufferData_->numSpotLights = static_cast<int32_t>((std::min)(spotLights.size(), static_cast<size_t>(kMaxSpotLights)));
	int spotIndex = 0;
	for (auto& [name, light] : spotLights)
	{
		if (spotIndex >= kMaxSpotLights) break;

		auto& gpuLight = lightBufferData_->spotLights[spotIndex];
		gpuLight.color = light.gpuData.color;
		gpuLight.position = light.gpuData.position;
		gpuLight.intensity = light.gpuData.intensity;
		gpuLight.direction = light.gpuData.direction;
		gpuLight.distance = light.gpuData.distance;
		gpuLight.decay = light.gpuData.decay;
		gpuLight.cosAngle = light.gpuData.cosAngle;
		gpuLight.cosFalloffStart = light.gpuData.cosFalloffStart;

		// シャドウ
		gpuLight.shadowEnabled = (light.shadowEnabled && shadowMapManager && shadowMapManager->HasSpotLightShadowMap(name)) ? 1 : 0;
		if (gpuLight.shadowEnabled)
		{
			gpuLight.shadowViewProj = light.viewProjectionMatrix;
		}

		++spotIndex;
	}

	// ポイントライト
	lightBufferData_->numPointLights = static_cast<int32_t>((std::min)(pointLights.size(), static_cast<size_t>(kMaxPointLights)));
	int pointIndex = 0;
	for (auto& [name, light] : pointLights)
	{
		if (pointIndex >= kMaxPointLights) break;

		auto& gpuLight = lightBufferData_->pointLights[pointIndex];
		gpuLight.color = light.gpuData.color;
		gpuLight.position = light.gpuData.position;
		gpuLight.intensity = light.gpuData.intensity;
		gpuLight.radius = light.gpuData.radius;
		gpuLight.decay = light.gpuData.decay;

		// シャドウ
		gpuLight.shadowEnabled = (light.shadowEnabled && shadowMapManager && shadowMapManager->HasPointLightShadowMap(name)) ? 1 : 0;
		if (gpuLight.shadowEnabled)
		{
			for (int face = 0; face < 6; ++face)
			{
				gpuLight.shadowViewProj[face] = light.viewProjectionMatrices[face];
			}
		}

		++pointIndex;
	}
}

void DeferredRenderer::ExecuteLightPass(
	GBuffer* gBuffer,
	Camera* camera,
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
	LightManager* lightManager,
	ShadowMapManager* shadowMapManager,
	FrameConstantAllocator* allocator
)
{
	if (!gBuffer || !lightManager)
	{
		return;
	}

	auto* commandList = dxCommon_->GetCommandList();

	// バッファ更新
	const D3D12_GPU_VIRTUAL_ADDRESS cameraAddress = UpdateCameraBuffer(camera, allocator);
	UpdateLightBuffer(lightManager, shadowMapManager);

	// レンダーターゲット設定
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	// ビューポート設定。ビューごとに解像度が違うため、G-Bufferの大きさに従う。
	D3D12_VIEWPORT viewport = {};
	viewport.Width = static_cast<float>(gBuffer->GetWidth());
	viewport.Height = static_cast<float>(gBuffer->GetHeight());
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	commandList->RSSetViewports(1, &viewport);

	D3D12_RECT scissorRect = {};
	scissorRect.right = static_cast<LONG>(gBuffer->GetWidth());
	scissorRect.bottom = static_cast<LONG>(gBuffer->GetHeight());
	commandList->RSSetScissorRects(1, &scissorRect);

	// パイプライン設定
	lightPassPipeline_->SetPipeline();

	// CBV設定
	// 0: CameraData
	commandList->SetGraphicsRootConstantBufferView(0, cameraAddress);
	// 1: DirectionalLightData
	commandList->SetGraphicsRootConstantBufferView(1, lightManager->GetDirectionalLightGPUAddress());
	// 2: CascadeShadowData
	commandList->SetGraphicsRootConstantBufferView(2, lightManager->GetCascadeShadowDataGPUAddress());
	// 3: LightBuffer (SpotLights + PointLights)
	commandList->SetGraphicsRootConstantBufferView(3, lightBuffer_->GetGPUVirtualAddress());

	// 4: G-Buffer SRV Table
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = srvManager_->GetGPUDescriptorHandle(gBuffer->GetSRVIndex(0));
	commandList->SetGraphicsRootDescriptorTable(4, srvHandle);

	// 5-8: Cascade Shadow Maps
	if (shadowMapManager && shadowMapManager->HasCascadeShadowMaps())
	{
		auto& cascadeShadowMap = shadowMapManager->GetCascadeShadowMap();
		for (uint32_t i = 0; i < 4; ++i)
		{
			srvManager_->SetGraphicsRootDescriptorTable(5 + i, cascadeShadowMap.srvIndices[i]);
		}
	}

	// 9-16: SpotLight Shadow Maps (8個)
	auto& spotLights = lightManager->GetSpotLights();
	int spotIndex = 0;
	for (auto& [name, light] : spotLights)
	{
		if (spotIndex >= kMaxSpotLights) break;
		if (light.shadowEnabled && shadowMapManager && shadowMapManager->HasSpotLightShadowMap(name))
		{
			auto& shadowMap = shadowMapManager->GetSpotLightShadowMap(name);
			srvManager_->SetGraphicsRootDescriptorTable(9 + spotIndex, shadowMap.srvIndex);
		}
		++spotIndex;
	}

	// 17-18: PointLight Shadow Maps (Cubemaps)
	auto& pointLights = lightManager->GetPointLights();
	int pointIndex = 0;
	for (auto& [name, light] : pointLights)
	{
		if (pointIndex >= kMaxPointLights) break;
		if (light.shadowEnabled && shadowMapManager && shadowMapManager->HasPointLightShadowMap(name))
		{
			auto& shadowMap = shadowMapManager->GetPointLightShadowMap(name);
			srvManager_->SetGraphicsRootDescriptorTable(17 + pointIndex, shadowMap.srvIndex);
		}
		++pointIndex;
	}

	// 19: トゥーン（NPR）の全体設定
	if (toonData_)
	{
		*toonData_ = toonSettings_;
		commandList->SetGraphicsRootConstantBufferView(19, toonBuffer_->GetGPUVirtualAddress());
	}

	// フルスクリーンクワッド描画
	commandList->DrawInstanced(4, 1, 0, 0);
}
} // namespace KCE
