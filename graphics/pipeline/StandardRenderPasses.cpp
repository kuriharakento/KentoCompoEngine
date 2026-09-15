#include "graphics/pipeline/StandardRenderPasses.h"

#include "base/Camera.h"
#include "base/DirectXCommon.h"
#include "base/RenderTexture.h"
#include "effects/particle/ParticleManager.h"
#include "graphics/2d/SpriteCommon.h"
#include "graphics/2d/TextOverlay.h"
#include "graphics/text/Text3DRenderer.h"
#include "graphics/3d/Object3dCommon.h"
#include "graphics/3d/Skybox.h"
#include "graphics/deferred/DeferredRenderer.h"
#include "graphics/deferred/GBuffer.h"
#include "graphics/atmosphere/BeamRenderer.h"
#include "graphics/atmosphere/FogRenderer.h"
#include "graphics/atmosphere/VolumetricLightRenderer.h"
#include "graphics/postfx/DepthOfFieldRenderer.h"
#include "graphics/postfx/FxaaRenderer.h"
#include "graphics/view/PlanarReflection.h"
#include "graphics/npr/OutlineRenderer.h"
#include "graphics/view/RenderView.h"
#include "graphics/shadow/ShadowMapPipeline.h"
#include "gameobject/base/GameObject.h"
#include "gameobject/manager/GameObjectManager.h"
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
constexpr uint64_t kShadowHashOffset = 14695981039346656037ull;
constexpr uint64_t kShadowHashPrime = 1099511628211ull;

void HashShadowBytes(uint64_t& hash, const void* data, size_t size)
{
	const auto* bytes = static_cast<const uint8_t*>(data);
	for (size_t index = 0; index < size; ++index)
	{
		hash ^= bytes[index];
		hash *= kShadowHashPrime;
	}
}

template<class Value>
void HashShadowValue(uint64_t& hash, const Value& value)
{
	HashShadowBytes(hash, &value, sizeof(value));
}

void HashShadowCaster(uint64_t& hash, const GameObject& object, bool& hasAnimatedCaster)
{
	const auto* renderable = object.GetRenderable3d();
	const bool isActive = object.IsActive() && !object.IsPendingDestroy();
	bool castsShadow = renderable != nullptr;
	if (const auto* object3d = object.GetObject3d())
	{
		castsShadow = object3d->GetCastShadow();
	}

	HashShadowValue(hash, &object);
	HashShadowValue(hash, renderable);
	HashShadowValue(hash, isActive);
	HashShadowValue(hash, castsShadow);
	if (isActive && castsShadow)
	{
		HashShadowValue(hash, object.GetPosition());
		HashShadowValue(hash, object.GetRotation());
		HashShadowValue(hash, object.GetScale());
		HashShadowValue(hash, object.GetModel());
		hasAnimatedCaster = hasAnimatedCaster || object.GetSkinnedObject3d() != nullptr;
	}

	for (const auto& [name, child] : object.GetChildren())
	{
		if (child)
		{
			HashShadowCaster(hash, *child, hasAnimatedCaster);
		}
	}
}

uint64_t GetShadowCasterState(bool& hasAnimatedCaster)
{
	uint64_t hash = kShadowHashOffset;
	hasAnimatedCaster = false;
	if (!GameObjectManager::HasInstance())
	{
		return hash;
	}

	const auto& objects = GameObjectManager::GetInstance()->GetGameObjects();
	HashShadowValue(hash, objects.size());
	for (const GameObject* object : objects)
	{
		if (object)
		{
			HashShadowCaster(hash, *object, hasAnimatedCaster);
		}
	}
	return hash;
}

uint64_t GetSpotLightShadowState(const CPUSpotLight& light)
{
	uint64_t hash = kShadowHashOffset;
	HashShadowValue(hash, light.gpuData.position);
	HashShadowValue(hash, light.gpuData.direction);
	HashShadowValue(hash, light.gpuData.distance);
	HashShadowValue(hash, light.gpuData.cosAngle);
	return hash;
}
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
	ctx.shadowMapManager->BeginFrame();

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
	bool hasAnimatedCaster = false;
	const uint64_t casterState = GetShadowCasterState(hasAnimatedCaster);
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

		const uint64_t lightState = GetSpotLightShadowState(light);
		if (!hasAnimatedCaster &&
			!ctx.shadowMapManager->NeedsSpotLightShadowRedraw(name, lightState, casterState))
		{
			continue;
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
		ctx.shadowMapManager->MarkSpotLightShadowRedrawn(name, lightState, casterState);
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
		ctx.shadowMapManager,
		ctx.frameConstantAllocator);
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

	// 線を省くビューでは、このビューの Draw3D で積まれた分を描かずに捨てる。
	// 残すと次の本編でもう一度描かれて、線が二重に見える
	if (ctx.view && !ctx.view->IsPassEnabled(RenderViewPass::DebugLines))
	{
		LineManager::GetInstance()->Clear();
		return;
	}

#ifdef _DEBUG
	// デバッグライン描画
	if (ctx.lightManager)
	{
		ctx.lightManager->DrawDebugLines();
	}
	if (ctx.cameraManager)
	{
		ctx.cameraManager->DrawDebugLines();
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

bool Text3DPass::ShouldExecute(const RenderPassContext& ctx) const
{
	return IsEnabled() && (!ctx.view || ctx.view->IsPassEnabled(RenderViewPass::Text3D));
}

bool OutlinePass::ShouldExecute(const RenderPassContext& ctx) const
{
	return IsEnabled() && (!ctx.view || ctx.view->IsPassEnabled(RenderViewPass::Outline));
}

bool FogPass::ShouldExecute(const RenderPassContext& ctx) const
{
	return IsEnabled() && (!ctx.view || ctx.view->IsPassEnabled(RenderViewPass::Fog));
}

bool BeamPass::ShouldExecute(const RenderPassContext& ctx) const
{
	return IsEnabled() && (!ctx.view || ctx.view->IsPassEnabled(RenderViewPass::Beams));
}

void Text3DPass::Execute(const RenderPassContext& ctx)
{
	if (!ctx.text3DRenderer || !ctx.view || !ctx.view->IsValid() || !ctx.cameraManager)
	{
		return;
	}
	// サブビューの描画中はアクティブカメラが差し替わっているので、そのビューのカメラになる
	ctx.text3DRenderer->Draw(
		ctx.cameraManager->GetActiveCamera(),
		ctx.view->GetSceneColor()->GetRTVHandle(),
		ctx.view->GetGBuffer()->GetDSVHandle(),
		ctx.frameConstantAllocator);
}

void OutlinePass::Execute(const RenderPassContext& ctx)
{
	if (!ctx.outlineRenderer || !ctx.view || !ctx.view->IsValid() || !ctx.cameraManager)
	{
		return;
	}

	ctx.outlineRenderer->Draw(
		ctx.cameraManager->GetActiveCamera(),
		ctx.view->GetGBuffer(),
		ctx.view->GetSceneColor()->GetRTVHandle(),
		ctx.frameConstantAllocator);
}

void FogPass::Execute(const RenderPassContext& ctx)
{
	if (!ctx.fogRenderer || !ctx.view || !ctx.view->IsValid() || !ctx.cameraManager)
	{
		return;
	}

	// サブビューの描画中はアクティブカメラが差し替わっているので、そのビューのカメラになる
	ctx.fogRenderer->Draw(
		ctx.cameraManager->GetActiveCamera(),
		ctx.view->GetGBuffer(),
		ctx.view->GetSceneColor()->GetRTVHandle(),
		ctx.frameConstantAllocator);
}

void BeamPass::Execute(const RenderPassContext& ctx)
{
	if (!ctx.beamRenderer || !ctx.view || !ctx.view->IsValid() || !ctx.cameraManager)
	{
		return;
	}

	ctx.beamRenderer->Draw(
		ctx.cameraManager->GetActiveCamera(),
		ctx.view->GetGBuffer(),
		ctx.view->GetSceneColor()->GetRTVHandle(),
		ctx.lightManager,
		ctx.frameConstantAllocator);
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

void ReflectionPass::Execute(const RenderPassContext& ctx)
{
	if (!ctx.planarReflection || !ctx.view || !ctx.view->IsValid() || !ctx.cameraManager)
	{
		return;
	}
	ctx.planarReflection->Composite(
		ctx.cameraManager->GetActiveCamera(),
		ctx.view->GetGBuffer(),
		ctx.view->GetSceneColor()->GetRTVHandle(),
		ctx.frameConstantAllocator);
}

void VolumetricLightPass::Execute(const RenderPassContext& ctx)
{
	if (!ctx.volumetricLightRenderer || !ctx.view || !ctx.view->IsValid() || !ctx.cameraManager)
	{
		return;
	}
	ctx.volumetricLightRenderer->Draw(
		ctx.cameraManager->GetActiveCamera(),
		ctx.view->GetGBuffer(),
		ctx.view->GetSceneColor()->GetRTVHandle(),
		ctx.view->GetWidth(),
		ctx.view->GetHeight(),
		ctx.lightManager,
		ctx.shadowMapManager,
		ctx.beamRenderer,
		ctx.frameConstantAllocator);
}

void DepthOfFieldPass::Execute(const RenderPassContext& ctx)
{
	if (!ctx.depthOfFieldRenderer || !ctx.view || !ctx.view->IsValid() || !ctx.cameraManager)
	{
		return;
	}
	ctx.depthOfFieldRenderer->Draw(ctx.cameraManager->GetActiveCamera(), ctx.view, ctx.frameConstantAllocator);
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

	// FXAA を掛けるときは、一度 FXAA の入力へ描かせてから、均しながら本来の出力先へ書く。
	// 出力先が nullptr の場合はバックバッファへ直接描かれる
	const bool useFxaa = ctx.fxaaRenderer && ctx.fxaaRenderer->IsActive();
	ctx.postProcessManager->Draw(ctx.view->GetSceneColor(), useFxaa ? ctx.fxaaRenderer->GetInputTarget() : ctx.outputTarget);
	if (useFxaa)
	{
		ctx.fxaaRenderer->Apply(ctx.outputTarget, ctx.frameConstantAllocator);
	}
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

	// 歌詞や会話はシーンの 2D より手前に重ねる
	if (ctx.textOverlay)
	{
		ctx.textOverlay->Draw();
	}

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
	// 輪郭線は G-Buffer から拾うので、フォワード描画より前に引く。
	// 後に引くと、G-Buffer に何も書かないフォワードの物体の上に、その奥の物体の線が透けて出る
	pipeline.AddPass(std::make_unique<OutlinePass>());
	pipeline.AddPass(std::make_unique<ForwardOpaquePass>());
	pipeline.AddPass(std::make_unique<SkyboxPass>());
	// 反射は床の面の上なので霧より前、光の筋は空気なので半透明とビームの後
	pipeline.AddPass(std::make_unique<ReflectionPass>());
	pipeline.AddPass(std::make_unique<FogPass>());
	pipeline.AddPass(std::make_unique<TransparentPass>());
	pipeline.AddPass(std::make_unique<Text3DPass>());
	pipeline.AddPass(std::make_unique<BeamPass>());
	pipeline.AddPass(std::make_unique<VolumetricLightPass>());
	pipeline.AddPass(std::make_unique<SceneColorResolvePass>());
	// 被写界深度は光の筋や反射も含めてぼかしたいので、シーンが出来上がってから
	pipeline.AddPass(std::make_unique<DepthOfFieldPass>());
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
	// 輪郭線は G-Buffer から拾うので、フォワード描画より前に引く。
	// 後に引くと、G-Buffer に何も書かないフォワードの物体の上に、その奥の物体の線が透けて出る
	pipeline.AddPass(std::make_unique<OutlinePass>());
	pipeline.AddPass(std::make_unique<ForwardOpaquePass>());
	pipeline.AddPass(std::make_unique<SkyboxPass>());
	pipeline.AddPass(std::make_unique<FogPass>());
	pipeline.AddPass(std::make_unique<TransparentPass>());
	// モニターや床の反射にも文字が映るように、サブビューでも描く
	pipeline.AddPass(std::make_unique<Text3DPass>());
	pipeline.AddPass(std::make_unique<BeamPass>());
	pipeline.AddPass(std::make_unique<SceneColorResolvePass>());
}
} // namespace KCE
