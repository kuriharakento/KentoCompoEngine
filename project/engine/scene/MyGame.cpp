#include "MyGame.h"

#include <future>
#include <chrono>
#include "manager/graphics/ModelManager.h"
#include "base/Logger.h"
#include "effects/particle/ParticleManager.h"
#include "ImGui/imgui_internal.h"
#include "manager/graphics/TextureManager.h"
#include "manager/graphics/LineManager.h"

///=============================================================================
///						初期化・終了処理
///=============================================================================

void MyGame::Initialize()
{
	// フレームワークの初期化
	Framework::Initialize();

	// シーンコンテキストの作成
	SceneContext context;
	context = {
		spriteCommon_.get(),
		objectCommon_.get(),
		cameraManager_.get(),
		lightManager_.get(),
		postProcessManager_.get(),
		skybox_.get(),
		shadowMapManager_.get(),
	};

	// テクスチャの読み込み
	LoadTextures();

	// モデルの読み込み
	LoadModels();

	// ゲームの初期化処理
	sceneManager_->Initialize(context);

	// Skyboxの初期化
	skybox_->Initialize(dxCommon_.get(), "./Resources/skybox.dds");

	// シーン描画用レンダーテクスチャ（ポストプロセス後）の初期化
	constexpr float kClearColorValue = 0.1f;  // 暗いグレー
	Vector4 clearColor = { kClearColorValue, kClearColorValue, kClearColorValue, 1.0f };
	sceneRenderTexture_ = std::make_unique<RenderTexture>();
	sceneRenderTexture_->Initialize(
		dxCommon_.get(),
		srvManager_.get(),
		WinApp::kClientWidth,
		WinApp::kClientHeight,
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		clearColor
	);
}

void MyGame::Finalize()
{
	// ゲームの終了処理
	sceneManager_.reset();

	// フレームワークの終了処理
	Framework::Finalize();
}

///=============================================================================
///						更新処理
///=============================================================================

void MyGame::Update()
{
	// フレームワークの更新処理
	Framework::Update();

	// パフォーマンス情報の表示
	Framework::ShowPerformanceInfo();

	// ゲームの更新処理
	sceneManager_->Update();

	// パーティクルマネージャーの更新
	ParticleManager::GetInstance()->Update(cameraManager_.get());
}

///=============================================================================
///						描画処理
///=============================================================================

void MyGame::Draw()
{
	srvManager_->PreDraw();

	///--------------------------------------------------------------
	///						シャドウマップ生成パス
	///--------------------------------------------------------------

	// カスケードシャドウ行列を計算
	lightManager_->UpdateCascadeShadowMatrices(
		cameraManager_->GetActiveCamera(),
		kShadowNearPlane, kShadowFarPlane
	);

	// カスケードシャドウマップ描画（4カスケード）
	for (uint32_t cascade = 0; cascade < 4; ++cascade) {
		shadowMapManager_->BeginCascadeShadowPass(cascade);
		shadowMapPipeline_->SetPipeline();

		D3D12_GPU_VIRTUAL_ADDRESS cascadeMatrixAddr = lightManager_->GetCascadeLightViewProjectionGPUAddress(cascade);
		if (cascadeMatrixAddr != 0) {
			dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(0, cascadeMatrixAddr);
		}

		sceneManager_->DrawShadow();
		shadowMapManager_->EndShadowPass();
	}

	// スポットライトシャドウマップ描画
	auto& spotLights = lightManager_->GetSpotLights();
	for (auto& [name, light] : spotLights) {
		if (!light.shadowEnabled) continue;

		if (!shadowMapManager_->HasSpotLightShadowMap(name)) {
			shadowMapManager_->CreateSpotLightShadowMap(name);
		}

		shadowMapManager_->BeginSpotLightShadowPass(name);
		shadowMapPipeline_->SetPipeline();

		D3D12_GPU_VIRTUAL_ADDRESS spotMatrixAddr = lightManager_->GetSpotLightShadowMatrixGPUAddress(name);
		if (spotMatrixAddr != 0) {
			dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(0, spotMatrixAddr);
		}

		sceneManager_->DrawShadow();
		shadowMapManager_->EndShadowPass();
	}

	// ポイントライトシャドウマップ描画（6面キューブマップ）
	auto& pointLights = lightManager_->GetPointLights();
	for (auto& [name, light] : pointLights) {
		if (!light.shadowEnabled) continue;

		if (!shadowMapManager_->HasPointLightShadowMap(name)) {
			shadowMapManager_->CreatePointLightShadowMap(name);
		}

		lightManager_->UpdatePointLightShadowMatrix(name, kShadowNearPlane, light.gpuData.radius);

		for (uint32_t face = 0; face < 6; ++face) {
			shadowMapManager_->BeginPointLightShadowPass(name, face);
			shadowMapPipeline_->SetPipeline();

			D3D12_GPU_VIRTUAL_ADDRESS pointMatrixAddr = lightManager_->GetPointLightShadowMatrixGPUAddress(name, face);
			if (pointMatrixAddr != 0) {
				dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(0, pointMatrixAddr);
			}

			sceneManager_->DrawShadow();
			shadowMapManager_->EndShadowPass();
		}
	}

	///--------------------------------------------------------------
	///						ディファードレンダリング
	///--------------------------------------------------------------

	// G-Bufferパス
	deferredRenderer_->BeginGeometryPass();
	sceneManager_->DrawGBuffer();
	deferredRenderer_->EndGeometryPass();

	// ライトパス
	renderTexture_->BeginRender();
	deferredRenderer_->ExecuteLightPass(
		renderTexture_->GetRTVHandle(),
		cameraManager_.get(),
		lightManager_.get(),
		shadowMapManager_.get()
	);

	///--------------------------------------------------------------
	///						フォワードレンダリング
	///--------------------------------------------------------------

	// 3D共通設定
	Framework::Draw3DSetting();

	// 深度バッファを書き込み可能状態に遷移
	deferredRenderer_->GetGBuffer()->TransitionDepthToDepthWrite();

	// レンダーターゲットと深度バッファを設定
	auto dsvHandle = deferredRenderer_->GetGBuffer()->GetDSVHandle();
	auto rtvHandle = renderTexture_->GetRTVHandle();
	dxCommon_->GetCommandList()->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// シャドウマップリソースをバインド
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(10, lightManager_->GetShadowMatrixGPUAddress());
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(11, lightManager_->GetCascadeShadowDataGPUAddress());

	if (shadowMapManager_->GetCascadeShadowMap().isEnabled) {
		for (uint32_t i = 0; i < ShadowMapConfig::kCascadeCount; ++i) {
			dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(
				12 + i,
				srvManager_->GetGPUDescriptorHandle(shadowMapManager_->GetCascadeShadowMap().srvIndices[i])
			);
		}
	}

	// フォワードパス対象オブジェクトの描画
	sceneManager_->Draw3D();

	// デバッグライン描画
	lightManager_->DrawDebugLines();
	LineManager::GetInstance()->RenderLines();

	// Skybox描画
	skybox_->Draw();

	// パーティクル描画
	ParticleManager::GetInstance()->Draw();

	///--------------------------------------------------------------
	///						2D描画
	///--------------------------------------------------------------

	Framework::Draw2DSetting();
	sceneManager_->Draw2D();

	// 深度バッファをSRV状態に戻す
	deferredRenderer_->GetGBuffer()->TransitionDepthToSRV();

	renderTexture_->EndRender();

	///--------------------------------------------------------------
	///						ポストプロセス & ImGui
	///--------------------------------------------------------------

#ifdef USE_IMGUI
	// ポストプロセス処理
	sceneRenderTexture_->BeginRender();
	postProcessManager_->Draw(renderTexture_.get(), sceneRenderTexture_.get());
	sceneRenderTexture_->EndRender();

	// バックバッファのクリア
	dxCommon_->PreDraw();

	// ImGui ドッキングスペースの作成
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, kImGuiWindowRounding);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, kImGuiWindowBorderSize);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kImGuiWindowRounding, kImGuiWindowRounding));
	ImGui::Begin("DockSpace", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground);
	ImGui::PopStyleVar(3);
	ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
	ImGui::End();

	// シーンウィンドウ
	ImGui::Begin("Scene");
	ImVec2 viewportSize = ImGui::GetContentRegionAvail();
	ImGui::Image((ImTextureID)sceneRenderTexture_->GetGPUHandle().ptr, viewportSize);
	ImGui::End();

	// その他のウィンドウ（プレースホルダー）
	ImGui::Begin("Hierarchy");
	ImGui::Text("Hierarchy Placeholder");
	ImGui::End();

	ImGui::Begin("Inspector");
	ImGui::Text("Inspector Placeholder");
	ImGui::End();

	ImGui::Begin("Project");
	ImGui::Text("Project Placeholder");
	ImGui::End();

	ImGui::Begin("Console");
	ImGui::Text("Console Placeholder");
	ImGui::End();
#else
	// バックバッファのクリア
	dxCommon_->PreDraw();

	// ポストプロセス処理
	postProcessManager_->Draw(renderTexture_.get(), nullptr);
#endif

	imguiManager_->End();
	imguiManager_->Draw();
	dxCommon_->PostDraw();
}

///=============================================================================
///						リソース読み込み
///=============================================================================

void MyGame::LoadTextures()
{
	TextureManager::GetInstance()->LoadTexture("./Resources/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/black.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/red.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/testSprite.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/white1x1.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/monsterBall.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/gradationLine.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/gradation.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/circle2.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/flowerfun.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/star.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/skybox.dds");
	TextureManager::GetInstance()->LoadTexture("./Resources/minimap_frame.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/numbers.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/title_logo.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/UI/hp_bar_fill.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/UI/hp_bar_frame.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/UI/reticle.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/UI/dot_reticle.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/UI/retry.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/UI/back_to_title.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/fonts/luna_atlas.png");
}

void MyGame::LoadModels()
{
	ModelManager::GetInstance()->LoadModel("multimaterial");
	ModelManager::GetInstance()->LoadModel("multimesh");
	ModelManager::GetInstance()->LoadModel("cube");
	ModelManager::GetInstance()->LoadModel("terrain");
	ModelManager::GetInstance()->LoadModel("skydome");
	ModelManager::GetInstance()->LoadModel("bullet");
	ModelManager::GetInstance()->LoadModel("wall");
	ModelManager::GetInstance()->LoadModel("player");
	ModelManager::GetInstance()->LoadModel("enemy");
	ModelManager::GetInstance()->LoadModel("plane", ".gltf");
	ModelManager::GetInstance()->LoadModel("walk", ".gltf");
	ModelManager::GetInstance()->LoadModel("street", ".gltf");
}
