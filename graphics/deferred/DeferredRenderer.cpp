#include "DeferredRenderer.h"
#include "base/DirectXCommon.h"
#include "manager/system/SrvManager.h"
#include "manager/scene/LightManager.h"
#include "manager/graphics/ShadowMapManager.h"
#include "manager/scene/CameraManager.h"
#include "base/Camera.h"
#include "base/Logger.h"
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

	KCE::Logger::Log("DeferredRenderer initialized\n");
}

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

void DeferredRenderer::UpdateCameraBuffer(Camera* camera)
{
	if (!camera || !cameraData_) return;

	cameraData_->worldPos = camera->GetTranslate();
	cameraData_->viewMatrix = camera->GetViewMatrix();
	cameraData_->projMatrix = camera->GetProjectionMatrix();
	cameraData_->invViewMatrix = Inverse(cameraData_->viewMatrix);
	cameraData_->invProjMatrix = Inverse(cameraData_->projMatrix);
	cameraData_->nearPlane = 0.1f;
	cameraData_->farPlane = 200.0f;
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
	ShadowMapManager* shadowMapManager
)
{
	if (!gBuffer || !lightManager)
	{
		return;
	}

	auto* commandList = dxCommon_->GetCommandList();

	// バッファ更新
	UpdateCameraBuffer(camera);
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
	commandList->SetGraphicsRootConstantBufferView(0, cameraBuffer_->GetGPUVirtualAddress());
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

	// フルスクリーンクワッド描画
	commandList->DrawInstanced(4, 1, 0, 0);
}
} // namespace KCE
