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
	};

	// テクスチャの読み込み
	LoadTextures();

	// モデルの読み込み
	LoadModels();

	//ゲームの初期化処理
	sceneManager_->Initialize(context);

	// Skyboxの初期化
	skybox_->Initialize(dxCommon_.get(), "./Resources/skybox.dds");
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
	/*----[ オフスクリーン描画 ]----*/

	renderTexture_->BeginRender();

	srvManager_->PreDraw();

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
	dxCommon_->PreDraw();
	postProcessManager_->Draw(renderTexture_.get());
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
