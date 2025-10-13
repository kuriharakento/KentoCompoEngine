#include "TitleScene.h"

// audio

// scene
#include "engine/scene/manager/SceneManager.h"
// editor

// math

// graphics
#include "input/Input.h"
#include "manager/graphics/LineManager.h"
// app

// components


void TitleScene::Initialize()
{
	// カメラ設定
	sceneManager_->GetCameraManager()->GetActiveCamera()->SetTranslate(Vector3(0.0f, 1.5f, -15.0f));

	// タイトルロゴの生成
	titleLogo_ = std::make_unique<Sprite>();
	titleLogo_->Initialize(sceneManager_->GetSpriteCommon(), "./Resources/title_logo.png");
	titleLogo_->SetPosition({ 640.0f, 100.0f });
	titleLogo_->SetAnchorPoint({ 0.5f, 0.5f });
	titleLogo_->SetSize({ 300.0f, 200.0f });

	// スカイドームの生成
	//スカイドームの生成
	skydome_ = std::make_unique<Object3d>();
	skydome_->Initialize(sceneManager_->GetObject3dCommon());
	skydome_->SetModel("skydome");
	skydome_->SetLightManager(sceneManager_->GetLightManager());
	skydome_->SetEnableLighting(true);
	skydome_->SetDirectionalLightIntensity(0.5f);
	//ディレクショナルライトを下から上に照らす
	skydome_->SetDirectionalLightDirection({ 0.0f, -1.0f, 0.0f });
	skydome_->SetScale({ 0.8f, 0.8f, 0.8f });

	// 炎エフェクトの生成
	fireEffect_ = std::make_unique<TitleFireEffect>();
	fireEffect_->Initialize();

	// キューブの初期化
	cube_.center = Vector3(0.0f, 1.0f, 10.0f);
	cube_.size = Vector3(1.0f, 1.0f, 1.0f);
	cube_.rotate = MakeRotateYMatrix(0.0f);

	transitionEffect_.Initialize(
		sceneManager_->GetSpriteCommon(),
		"./Resources/black.png",
		18, 12,
		1280.0f, 720.0f
	);
}

void TitleScene::Finalize()
{

}

void TitleScene::Update()
{
	if (Input::GetInstance()->TriggerKey(DIK_SPACE))
	{
		sceneManager_->ChangeScene("GAMEPLAY");
	}

	// カメラの更新
	auto camera = sceneManager_->GetCameraManager()->GetActiveCamera();
	camera->SetTranslate(camera->GetTranslate() + Vector3(0.0f, 0.0f, 0.1f));
	if (camera->GetTranslate().z >= 100.0f)
	{
		camera->SetTranslate({ 0.0f, 1.5f, -15.0f });
	}

	// シーン遷移エフェクトの更新
	transitionEffect_.Update();

	// 炎エフェクトの更新
	fireEffect_->Update(camera->GetTranslate());

	// タイトルロゴの更新
	titleLogo_->Update();
	// スカイドームの更新
	skydome_->Update(sceneManager_->GetCameraManager());

	// キューブの更新
	cube_.center = camera->GetTranslate() + Vector3(0.0f, -1.0f, 10.0f);

	// キューブの上下動（sinf波）
	cubeWaveTime += 0.05f; // 波の速さ
	float baseY = 1.0f;    // 基準高さ
	float amplitude = 0.5f; // 振幅
	cube_.center.y = baseY + amplitude * sinf(cubeWaveTime);

	// キューブの回転
	cubeRotateY += 0.07f;
	if (cubeRotateY >= 3.14f)
	{
		cubeRotateY = 0.0f;
	}
	cube_.rotate = MakeRotateYMatrix(cubeRotateY);
}

void TitleScene::Draw3D()
{
	// グリッドを表示
	LineManager::GetInstance()->DrawGrid(
		600.0f,
		5.0f, 
		VectorColorCodes::DarkGray
	);

	// キューブの描画
	LineManager::GetInstance()->DrawOBB(
		cube_,
		VectorColorCodes::Cyan
	);

	// スカイドームの描画
	skydome_->Draw();
}

void TitleScene::Draw2D()
{
	titleLogo_->Draw();

	transitionEffect_.Draw();
}

void TitleScene::DrawImGui()
{

}
