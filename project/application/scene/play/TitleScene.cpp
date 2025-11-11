#include "TitleScene.h"

// audio
#include "audio/Audio.h"
// scene
#include "engine/scene/manager/SceneManager.h"
// input
#include "input/Input.h"
// graphics / manager
#include "manager/effect/PostProcessManager.h"
#include "manager/graphics/LineManager.h"

void TitleScene::Initialize()
{
	// BGM再生
	Audio::GetInstance()->LoadWave("title_bgm", "bgm/title.wav", SoundGroup::BGM);
	Audio::GetInstance()->PlayWave("title_bgm", true);
	Audio::GetInstance()->SetVolume("title_bgm", 0.2f);

	// スペースを押したときの効果音
	Audio::GetInstance()->LoadWave("start_se", "se/tap.wav", SoundGroup::SE);

	// カメラ設定
	sceneManager_->GetCameraManager()->GetActiveCamera()->SetTranslate(Vector3(0.0f, 1.5f, -15.0f));
	sceneManager_->GetCameraManager()->GetActiveCamera()->SetRotate(Vector3());

	// タイトルロゴの生成
	titleLogo_ = std::make_unique<Sprite>();
	titleLogo_->Initialize(sceneManager_->GetSpriteCommon(), "./Resources/title_logo.png");
	titleLogo_->SetPosition({ 640.0f, 100.0f });
	titleLogo_->SetAnchorPoint({ 0.5f, 0.5f });
	titleLogo_->SetSize({ 300.0f, 200.0f });

	// スカイドームの生成
	skydome_ = std::make_unique<Object3d>();
	skydome_->Initialize(sceneManager_->GetObject3dCommon());
	skydome_->SetModel("skydome");
	skydome_->SetLightManager(sceneManager_->GetLightManager());
	skydome_->SetEnableLighting(true);
	skydome_->SetDirectionalLightIntensity(0.5f);
	// ディレクショナルライトを下から上に照らす
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
		22, 16,
		1280.0f, 720.0f
	);

	// 色収差を有効化
	sceneManager_->GetPostProcessManager()->crtEffect_->SetEnabled(true);
	sceneManager_->GetPostProcessManager()->crtEffect_->SetCrtEnabled(true);
	sceneManager_->GetPostProcessManager()->crtEffect_->SetChromaticAberrationEnabled(true);
	sceneManager_->GetPostProcessManager()->crtEffect_->SetChromaticAberrationOffset(10.0f);

	// 初期状態は Enter（シーン開始時のフェード等を行う）
	StartState(SceneState::Playing);
}

void TitleScene::Finalize()
{
	// 色収差を無効化
	sceneManager_->GetPostProcessManager()->crtEffect_->SetEnabled(false);

	// BGM停止
	Audio::GetInstance()->StopWave("title_bgm");
}

// Playing（タイトル待機状態。ボタン入力で遷移）
void TitleScene::OnEnterPlaying()
{
}

void TitleScene::OnUpdatePlaying()
{
	// ImGui の描画（従来 Update の先頭で呼んでいたもの）
	DrawImGui();

	// スタート入力処理（スペース）
	if (Input::GetInstance()->TriggerKey(DIK_SPACE))
	{
		// スタート音再生
		Audio::GetInstance()->PlayWave("start_se", false);

		// 遷移時の演出スタート
		transitionEffect_.SetEaseType(SceneTransitionEase::InSine);
		transitionEffect_.SetFadeType(FadeType::FadeIn);
		transitionEffect_.SetMode(TransitionMode::LeftTopToRightBottom);
		transitionEffect_.Start(
			1.0f,
			VectorColorCodes::Red,
			VectorColorCodes::Black
		);

		// フェードアウト状態へ遷移して Exit 側で完了待ちする
		ChangeState(SceneState::Exit);
	}

	// カメラの更新（タイトル用の軽い動き）
	auto camera = sceneManager_->GetCameraManager()->GetActiveCamera();
	camera->SetTranslate(camera->GetTranslate() + Vector3(0.0f, 0.0f, 0.1f));
	if (camera->GetTranslate().z >= 100.0f)
	{
		camera->SetTranslate({ 0.0f, 1.5f, -15.0f });
	}

	// 各種更新（エフェクト・ロゴ・スカイドーム等）
	transitionEffect_.Update(); // Playing 時は特別なトランジションが無くても Update を呼ぶことで安定化
	fireEffect_->Update(camera->GetTranslate());
	titleLogo_->Update();
	skydome_->Update(sceneManager_->GetCameraManager());

	// キューブの更新
	cube_.center = camera->GetTranslate() + Vector3(0.0f, -1.0f, 10.0f);

	// キューブの上下動（sinf波）
	cubeWaveTime += 0.05f;
	float baseY = 1.0f;
	float amplitude = 0.5f;
	cube_.center.y = baseY + amplitude * sinf(cubeWaveTime);

	// キューブの回転
	cubeRotateY += 0.07f;
	if (cubeRotateY >= 3.14f)
	{
		cubeRotateY = 0.0f;
	}
	cube_.rotate = MakeRotateYMatrix(cubeRotateY);
}

void TitleScene::OnExitPlaying()
{
}

// Exit（遷移フェード）
void TitleScene::OnEnterExit()
{
}

void TitleScene::OnUpdateExit()
{
	// フェード進行処理（Exit 状態）
	transitionEffect_.Update();

	if (transitionEffect_.GetState() == TransitionState::Done)
	{
		// ゲームプレイシーンへ切り替え
		if (sceneManager_) sceneManager_->ChangeScene(SceneNames::GamePlay);
	}
}

void TitleScene::OnExitExit()
{
}

// ----------------------------------------------------------------
// 描画
// ----------------------------------------------------------------

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
#ifdef USE_IMGUI
	ImGui::Begin("Title Scene");
#pragma region PostProcess

	#pragma endregion
	ImGui::End();
#endif
}