#include "graphics/pipeline/StandardRenderPasses.h"

#include "base/Camera.h"
#include "base/DirectXCommon.h"
#include "base/RenderTexture.h"
#include "effects/particle/ParticleManager.h"
#include "graphics/2d/SpriteCommon.h"
#include "graphics/3d/Object3dCommon.h"
#include "graphics/3d/Skybox.h"
#include "graphics/deferred/DeferredRenderer.h"
#include "graphics/deferred/GBuffer.h"
#include "graphics/view/RenderView.h"
#include "graphics/shadow/ShadowMapPipeline.h"
#include "manager/effect/PostProcessManager.h"
#include "manager/graphics/LineManager.h"
#include "manager/graphics/ShadowMapManager.h"
#include "manager/scene/CameraManager.h"
#include "manager/scene/LightManager.h"
#include "manager/system/SrvManager.h"
#include "scene/manager/SceneManager.h"

namespace KCE
{
namespace
{
/** @brief シャドウ行列のCBVを差すルートパラメータ番号 */
constexpr UINT kRootParamShadowMatrix = 10;
/** @brief カスケードシャドウデータのCBVを差すルートパラメータ番号 */
constexpr UINT kRootParamCascadeShadowData = 11;
/** @brief カスケードシャドウマップのSRVを差す先頭のルートパラメータ番号（t6〜t9） */
constexpr UINT kRootParamCascadeShadowSrvBase = 12;
/** @brief シャドウマップ描画時に行列を差すルートパラメータ番号 */
constexpr UINT kRootParamShadowPassMatrix = 0;
/** @brief ポイントライトのキューブマップの面数 */
constexpr uint32_t kPointLightFaceCount = 6;
} // namespace

void ApplyCommon3DRenderingSetting(const RenderPassContext& ctx)
{
	if (ctx.objectCommon)
	{
		ctx.objectCommon->CommonRenderingSetting();
	}

	if (!ctx.shadowMapManager || !ctx.shadowMapManager->HasCascadeShadowMaps() || !ctx.srvManager)
	{
		return;
	}

	auto& cascadeShadowMap = ctx.shadowMapManager->GetCascadeShadowMap();

	// 4つのカスケードシャドウマップSRVをバインドする
	for (uint32_t i = 0; i < ShadowMapConfig::kCascadeCount; ++i)
	{
		ctx.srvManager->SetGraphicsRootDescriptorTable(kRootParamCascadeShadowSrvBase + i, cascadeShadowMap.srvIndices[i]);
	}

	if (!ctx.lightManager || !ctx.dxCommon)
	{
		return;
	}

	const D3D12_GPU_VIRTUAL_ADDRESS cascadeDataAddress = ctx.lightManager->GetCascadeShadowDataGPUAddress();
	if (cascadeDataAddress != 0)
	{
		ctx.dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(kRootParamCascadeShadowData, cascadeDataAddress);
	}
}

void ApplyCommon2DRenderingSetting(const RenderPassContext& ctx)
{
	if (ctx.spriteCommon)
	{
		ctx.spriteCommon->CommonRenderingSetting();
	}
}

///=============================================================================
///						シャドウマップ生成パス
///=============================================================================

void ShadowMapPass::Execute(const RenderPassContext& ctx)
{
	if (!ctx.lightManager || !ctx.shadowMapManager || !ctx.shadowMapPipeline || !ctx.cameraManager)
	{
		return;
	}

	auto* commandList = ctx.dxCommon->GetCommandList();

	// カスケードシャドウ行列を計算する
	ctx.lightManager->UpdateCascadeShadowMatrices(
		ctx.cameraManager->GetActiveCamera(),
		ctx.shadowNearPlane, ctx.shadowFarPlane);

	// カスケードシャドウマップ描画
	for (uint32_t cascade = 0; cascade < ShadowMapConfig::kCascadeCount; ++cascade)
	{
		ctx.shadowMapManager->BeginCascadeShadowPass(cascade);
		ctx.shadowMapPipeline->SetPipeline();

		const D3D12_GPU_VIRTUAL_ADDRESS cascadeMatrixAddress = ctx.lightManager->GetCascadeLightViewProjectionGPUAddress(cascade);
		if (cascadeMatrixAddress != 0)
		{
			commandList->SetGraphicsRootConstantBufferView(kRootParamShadowPassMatrix, cascadeMatrixAddress);
			ctx.shadowMapManager->SetCurrentShadowMatrixAddress(cascadeMatrixAddress);
		}

		ctx.sceneManager->DrawShadow();
		ctx.shadowMapManager->EndShadowPass();
	}

	// スポットライトシャドウマップ描画
	auto& spotLights = ctx.lightManager->GetSpotLights();
	for (auto& [name, light] : spotLights)
	{
		if (!light.shadowEnabled)
		{
			continue;
		}

		if (!ctx.shadowMapManager->HasSpotLightShadowMap(name))
		{
			ctx.shadowMapManager->CreateSpotLightShadowMap(name);
		}

		ctx.shadowMapManager->BeginSpotLightShadowPass(name);
		ctx.shadowMapPipeline->SetPipeline();

		const D3D12_GPU_VIRTUAL_ADDRESS spotMatrixAddress = ctx.lightManager->GetSpotLightShadowMatrixGPUAddress(name);
		if (spotMatrixAddress != 0)
		{
			commandList->SetGraphicsRootConstantBufferView(kRootParamShadowPassMatrix, spotMatrixAddress);
			ctx.shadowMapManager->SetCurrentShadowMatrixAddress(spotMatrixAddress);
		}

		ctx.sceneManager->DrawShadow();
		ctx.shadowMapManager->EndShadowPass();
	}

	// ポイントライトシャドウマップ描画（6面キューブマップ）
	auto& pointLights = ctx.lightManager->GetPointLights();
	for (auto& [name, light] : pointLights)
	{
		if (!light.shadowEnabled)
		{
			continue;
		}

		if (!ctx.shadowMapManager->HasPointLightShadowMap(name))
		{
			ctx.shadowMapManager->CreatePointLightShadowMap(name);
		}

		ctx.lightManager->UpdatePointLightShadowMatrix(name, ctx.shadowNearPlane, light.gpuData.radius);

		for (uint32_t face = 0; face < kPointLightFaceCount; ++face)
		{
			ctx.shadowMapManager->BeginPointLightShadowPass(name, face);
			ctx.shadowMapPipeline->SetPipeline();

			const D3D12_GPU_VIRTUAL_ADDRESS pointMatrixAddress = ctx.lightManager->GetPointLightShadowMatrixGPUAddress(name, face);
			if (pointMatrixAddress != 0)
			{
				commandList->SetGraphicsRootConstantBufferView(kRootParamShadowPassMatrix, pointMatrixAddress);
				ctx.shadowMapManager->SetCurrentShadowMatrixAddress(pointMatrixAddress);
			}

			ctx.sceneManager->DrawShadow();
			ctx.shadowMapManager->EndShadowPass();
		}
	}
}

///=============================================================================
///						ディファードレンダリング
///=============================================================================

void GBufferPass::Execute(const RenderPassContext& ctx)
{
	if (!ctx.deferredRenderer || !ctx.view)
	{
		return;
	}

	ctx.deferredRenderer->BeginGeometryPass(ctx.view->GetGBuffer());
	ctx.sceneManager->DrawGBuffer();
	ctx.deferredRenderer->EndGeometryPass(ctx.view->GetGBuffer());
}

void LightingPass::Execute(const RenderPassContext& ctx)
{
	if (!ctx.deferredRenderer || !ctx.view || !ctx.view->IsValid())
	{
		return;
	}

	RenderTexture* sceneColor = ctx.view->GetSceneColor();

	// ここでシーン用レンダーターゲットへの描画を開始する。
	// 以降のフォワード系パスは同じターゲットへ描き足していく。
	sceneColor->BeginRender();

	// ビューにカメラが割り当てられていなければアクティブカメラで描く
	Camera* camera = ctx.view->GetCamera();
	if (!camera && ctx.cameraManager)
	{
		camera = ctx.cameraManager->GetActiveCamera();
	}

	ctx.deferredRenderer->ExecuteLightPass(
		ctx.view->GetGBuffer(),
		camera,
		sceneColor->GetRTVHandle(),
		ctx.lightManager,
		ctx.shadowMapManager);
}

///=============================================================================
///						フォワードレンダリング
///=============================================================================

void ForwardOpaquePass::Execute(const RenderPassContext& ctx)
{
	if (!ctx.view || !ctx.view->IsValid())
	{
		return;
	}

	auto* commandList = ctx.dxCommon->GetCommandList();
	GBuffer* gBuffer = ctx.view->GetGBuffer();

	// 3D共通設定
	ApplyCommon3DRenderingSetting(ctx);

	// 深度バッファを書き込み可能状態に遷移する
	gBuffer->TransitionDepthToDepthWrite();

	// シーン用レンダーターゲットとG-Bufferの深度を束ねる
	auto dsvHandle = gBuffer->GetDSVHandle();
	auto rtvHandle = ctx.view->GetSceneColor()->GetRTVHandle();
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// シャドウマップリソースをバインドする
	if (ctx.lightManager)
	{
		commandList->SetGraphicsRootConstantBufferView(kRootParamShadowMatrix, ctx.lightManager->GetShadowMatrixGPUAddress());
		commandList->SetGraphicsRootConstantBufferView(kRootParamCascadeShadowData, ctx.lightManager->GetCascadeShadowDataGPUAddress());
	}

	if (ctx.shadowMapManager && ctx.shadowMapManager->GetCascadeShadowMap().isEnabled && ctx.srvManager)
	{
		for (uint32_t i = 0; i < ShadowMapConfig::kCascadeCount; ++i)
		{
			commandList->SetGraphicsRootDescriptorTable(
				kRootParamCascadeShadowSrvBase + i,
				ctx.srvManager->GetGPUDescriptorHandle(ctx.shadowMapManager->GetCascadeShadowMap().srvIndices[i]));
		}
	}

	// フォワードパス対象オブジェクトの描画
	ctx.sceneManager->Draw3D();

#ifdef _DEBUG
	// デバッグライン描画
	if (ctx.lightManager)
	{
		ctx.lightManager->DrawDebugLines();
	}
#endif

	LineManager::GetInstance()->RenderLines();
}

void SkyboxPass::Execute(const RenderPassContext& ctx)
{
	if (!ctx.skybox)
	{
		return;
	}

	// サブビューの描画中はアクティブカメラが差し替わっているので、
	// ここで引けばそのビューのカメラになる
	Camera* camera = ctx.cameraManager ? ctx.cameraManager->GetActiveCamera() : nullptr;
	ctx.skybox->Draw(camera, ctx.frameConstantAllocator);
}

void TransparentPass::Execute(const RenderPassContext& ctx)
{
	if (!ctx.view || !ctx.view->IsValid() || !ctx.objectCommon)
	{
		return;
	}

	// 現在のビューへ再バインドする。シーン描画中の深度はDEPTH_WRITE状態で、
	// 書き込みの有無はPSOで制御する。SRVからの遷移をここで重複させない。
	auto rtv = ctx.view->GetSceneColor()->GetRTVHandle();
	auto dsv = ctx.view->GetGBuffer()->GetDSVHandle();
	ctx.dxCommon->GetCommandList()->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
	ApplyCommon3DRenderingSetting(ctx);
	ctx.objectCommon->TransparentRenderingSetting();
	if (ctx.lightManager)
	{
		ctx.dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(kRootParamShadowMatrix, ctx.lightManager->GetShadowMatrixGPUAddress());
	}
	ctx.sceneManager->DrawTransparent();
	ParticleManager::GetInstance()->Draw();
}

void SceneColorResolvePass::Execute(const RenderPassContext& ctx)
{
	if (!ctx.view || !ctx.view->IsValid())
	{
		return;
	}

	// 深度バッファをSRV状態へ戻す
	ctx.view->GetGBuffer()->TransitionDepthToSRV();

	// シーン用レンダーターゲットを読める状態にする
	ctx.view->GetSceneColor()->EndRender();
}

///=============================================================================
///						ポストプロセスと2D
///=============================================================================

void SubViewRenderPass::Execute(const RenderPassContext& ctx)
{
	(void)ctx;
	if (callback_)
	{
		callback_();
	}
}

void BackBufferPreparePass::Execute(const RenderPassContext& ctx)
{
	// バックバッファをクリアして描画対象にする
	ctx.dxCommon->PreDraw();
}

void PostProcessPass::Execute(const RenderPassContext& ctx)
{
	if (!ctx.postProcessManager || !ctx.view || !ctx.view->IsValid())
	{
		return;
	}

	// 出力先が nullptr の場合はバックバッファへ直接描かれる
	ctx.postProcessManager->Draw(ctx.view->GetSceneColor(), ctx.outputTarget);
}

void Sprite2DPass::Execute(const RenderPassContext& ctx)
{
	// レンダーターゲットへ出力している場合、ポストプロセスが
	// EndRender を呼んだ後なので、クリアせずに描画対象へ戻す
	if (ctx.outputTarget)
	{
		ctx.outputTarget->PreDrawForImGui();
	}

	ApplyCommon2DRenderingSetting(ctx);
	ctx.sceneManager->Draw2D();

	if (ctx.outputTarget)
	{
		ctx.outputTarget->EndRender();
	}
}

///=============================================================================
///						標準パイプラインの構築
///=============================================================================

void BuildStandardRenderPipeline(RenderPipeline& pipeline)
{
	pipeline.Clear();

	pipeline.AddPass(std::make_unique<ShadowMapPass>());
	pipeline.AddPass(std::make_unique<GBufferPass>());
	pipeline.AddPass(std::make_unique<LightingPass>());
	pipeline.AddPass(std::make_unique<ForwardOpaquePass>());
	pipeline.AddPass(std::make_unique<SkyboxPass>());
	pipeline.AddPass(std::make_unique<TransparentPass>());
	pipeline.AddPass(std::make_unique<SceneColorResolvePass>());
	pipeline.AddPass(std::make_unique<SubViewRenderPass>());
	pipeline.AddPass(std::make_unique<BackBufferPreparePass>());
	pipeline.AddPass(std::make_unique<PostProcessPass>());
	pipeline.AddPass(std::make_unique<Sprite2DPass>());
}

void BuildSceneOnlyRenderPipeline(RenderPipeline& pipeline)
{
	pipeline.Clear();

	// シャドウマップは本編のパイプラインが作ったものをそのまま使う
	pipeline.AddPass(std::make_unique<GBufferPass>());
	pipeline.AddPass(std::make_unique<LightingPass>());
	pipeline.AddPass(std::make_unique<ForwardOpaquePass>());
	pipeline.AddPass(std::make_unique<SkyboxPass>());
	pipeline.AddPass(std::make_unique<TransparentPass>());
	pipeline.AddPass(std::make_unique<SceneColorResolvePass>());
}
} // namespace KCE
