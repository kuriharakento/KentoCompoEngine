#include "MyGame.h"

#include <future>
#include <chrono>
#include "manager/graphics/ModelManager.h"
#include "base/Logger.h"
#include "manager/effect/ParticleManager.h"
#include "ImGui/imgui_internal.h"
#include "manager/graphics/TextureManager.h"
#include "manager/graphics/LineManager.h"

void MyGame::Initialize()
{
	//フレームワークの初期化
	Framework::Initialize();

	//シーンコンテキストの作成
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

	//ゲームの初期化処理
	sceneManager_->Initialize(context);

	// Skyboxの初期化
	skybox_->Initialize(dxCommon_.get(), "./Resources/skybox.dds");

	// シーン描画用レンダーテクスチャ（ポストプロセス後）の初期化
	// DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: 標準的なsRGBフォーマット
	Vector4 clearColor = { 0.1f, 0.1f, 0.1f, 1.0f };
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
	//ゲームの終了処理
	sceneManager_.reset();
	//フレームワークの終了処理
	Framework::Finalize();
}

void MyGame::Update()
{
	//フレームワークの更新処理
	Framework::Update();

	//パフォーマンス情報の表示
	Framework::ShowPerformanceInfo();

	//ゲームの更新処理
	sceneManager_->Update();

	// パーティクルマネージャーの更新
	ParticleManager::GetInstance()->Update(cameraManager_.get());
}

void MyGame::Draw()
{
	/*----[ シャドウパス描画（オフスクリーン前に実行） ]----*/
	
	srvManager_->PreDraw();
	
	// シャドウパス開始
	shadowMapManager_->BeginDirectionalLightShadowPass();
	shadowMapPipeline_->SetPipeline();
	
	// シャドウ行列の更新（シーン中心を基準に、広い範囲をカバー）
	// 中心点はシーンの中心（固定）、正射影サイズを大きくして広範囲をカバー
	lightManager_->UpdateDirectionalLightShadowMatrix(
		cameraManager_->GetActiveCamera()->GetTranslate(),
		50.0f,
		0.1f, 200.0f
	);
	
	// 各シーンのシャドウ対象オブジェクトを描画
	sceneManager_->DrawShadow();
	
	// シャドウパス終了
	shadowMapManager_->EndShadowPass();

	/*----[ オフスクリーン描画 ]----*/

	renderTexture_->BeginRender();

	/////////////////< 描画ここから >////////////////////

	// ---------- 3D描画 ---------

	//3D描画用設定
	Framework::Draw3DSetting();

	//3Dオブジェクトの描画
	sceneManager_->Draw3D();

	//ラインの描画
	LineManager::GetInstance()->RenderLines();

	// Skyboxの描画
	skybox_->Draw();

	//パーティクルの描画
	ParticleManager::GetInstance()->Draw();
		
	// ---------- 2D描画 ---------

	//2D描画用設定
	Framework::Draw2DSetting();

	//スプライトの描画
	sceneManager_->Draw2D();

	/////////////////< 描画ここまで >////////////////////

	renderTexture_->EndRender();
	
#ifdef USE_IMGUI
	// ポストプロセス処理（シーンテクスチャ -> シーンレンダーテクスチャ）
	// バックバッファではなく、ImGui表示用のテクスチャに出力する
	sceneRenderTexture_->BeginRender();
	postProcessManager_->Draw(renderTexture_.get(), sceneRenderTexture_.get());
	sceneRenderTexture_->EndRender();

	// バックバッファのクリア
	dxCommon_->PreDraw();

	// ドッキングスペースの作成
	// エラー回避のため、手動でウィンドウを作成してDockSpaceを設定
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("DockSpace", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground);
	ImGui::PopStyleVar(3);
	ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
	ImGui::End();

	// シーンウィンドウの作成
	ImGui::Begin("Scene");
	
	// ウィンドウサイズに合わせて画像を表示
	ImVec2 viewportSize = ImGui::GetContentRegionAvail();
	// ポストプロセス後のテクスチャを表示
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
	// ポストプロセス処理（シーンテクスチャ -> バックバッファ）
	postProcessManager_->Draw(renderTexture_.get(), nullptr);
#endif

	imguiManager_->End();
	imguiManager_->Draw();
	dxCommon_->PostDraw();
}

void MyGame::LoadTextures()
{
	TextureManager::GetInstance()->LoadTexture("./Resources/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/black.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/red.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/testSprite.png");
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
	ModelManager::GetInstance()->LoadModel("cube");
	//ModelManager::GetInstance()->LoadModel("plane",".gltf");
	ModelManager::GetInstance()->LoadModel("terrain");
	ModelManager::GetInstance()->LoadModel("skydome");
	ModelManager::GetInstance()->LoadModel("bullet");
	ModelManager::GetInstance()->LoadModel("wall");
	ModelManager::GetInstance()->LoadModel("player");
	ModelManager::GetInstance()->LoadModel("enemy");
}
