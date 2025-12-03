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
	Audio::GetInstance()->LoadWave("title_bgm", "bgm/title.wav", SoundGroup::BGM);
	Audio::GetInstance()->PlayWave("title_bgm", true);
	Audio::GetInstance()->SetVolume("title_bgm", 0.2f);

	Audio::GetInstance()->LoadWave("start_se", "se/tap.wav", SoundGroup::SE);

	sceneManager_->GetCameraManager()->GetActiveCamera()->SetTranslate(Vector3(0.0f, 1.5f, -15.0f));
	sceneManager_->GetCameraManager()->GetActiveCamera()->SetRotate(Vector3());

	titleLogo_ = std::make_unique<Sprite>();
	titleLogo_->Initialize(sceneManager_->GetSpriteCommon(), "./Resources/title_logo.png");
	titleLogo_->SetPosition({ 640.0f, 100.0f });
	titleLogo_->SetAnchorPoint({ 0.5f, 0.5f });
	titleLogo_->SetSize({ 300.0f, 200.0f });

	skydome_ = std::make_unique<Object3d>();
	skydome_->Initialize(sceneManager_->GetObject3dCommon());
	skydome_->SetModel("skydome");
	skydome_->SetLightManager(sceneManager_->GetLightManager());
	skydome_->SetEnableLighting(true);
	skydome_->SetDirectionalLightIntensity(0.5f);
	skydome_->SetDirectionalLightDirection({ 0.0f, -1.0f, 0.0f });
	skydome_->SetScale({ 0.8f, 0.8f, 0.8f });

	fireEffect_ = std::make_unique<TitleFireEffect>();
	fireEffect_->Initialize();

	cube_.center = Vector3(0.0f, 1.0f, 10.0f);
	cube_.size = Vector3(1.0f, 1.0f, 1.0f);
	cube_.rotate = MakeRotateYMatrix(0.0f);

	transitionEffect_.Initialize(
		sceneManager_->GetSpriteCommon(),
		"./Resources/black.png",
		22, 16,
		1280.0f, 720.0f
	);

	// 色収差エフェクトを有効化してレトロ風の雰囲気を演出
	sceneManager_->GetPostProcessManager()->crtEffect_->SetEnabled(true);
	sceneManager_->GetPostProcessManager()->crtEffect_->SetCrtEnabled(true);
	sceneManager_->GetPostProcessManager()->crtEffect_->SetChromaticAberrationEnabled(true);
	sceneManager_->GetPostProcessManager()->crtEffect_->SetChromaticAberrationOffset(10.0f);

	// フォントスプライトの初期化（テスト用）
	fontSprite_ = std::make_unique<FontSprite>();
	fontSprite_->Initialize(
		sceneManager_->GetSpriteCommon(),
		"luna"
	);
	fontSprite_->SetText("Press SPACE to Start");
	fontSprite_->SetPosition({ 640.0f, 500.0f });
	fontSprite_->SetScale(0.1f);

	StartState(SceneState::Playing);
}

void TitleScene::Finalize()
{
	sceneManager_->GetPostProcessManager()->crtEffect_->SetEnabled(false);

	Audio::GetInstance()->StopWave("title_bgm");
}

// ==================================================
// Playing状態（タイトル表示・入力待ち）
// ==================================================
void TitleScene::OnEnterPlaying()
{
}

void TitleScene::OnUpdatePlaying()
{
	DrawImGui();

	fontSprite_->Update();

	if (Input::GetInstance()->TriggerKey(DIK_SPACE))
	{
		Audio::GetInstance()->PlayWave("start_se", false);

		transitionEffect_.SetEaseType(SceneTransitionEase::InSine);
		transitionEffect_.SetFadeType(FadeType::FadeIn);
		transitionEffect_.SetMode(TransitionMode::LeftTopToRightBottom);
		transitionEffect_.Start(
			1.0f,
			VectorColorCodes::Red,
			VectorColorCodes::Black
		);

		ChangeState(SceneState::Exit);
	}

	auto camera = sceneManager_->GetCameraManager()->GetActiveCamera();
	camera->SetTranslate(camera->GetTranslate() + Vector3(0.0f, 0.0f, 0.1f));
	if (camera->GetTranslate().z >= 100.0f)
	{
		camera->SetTranslate({ 0.0f, 1.5f, -15.0f });
	}

	transitionEffect_.Update();
	fireEffect_->Update(camera->GetTranslate());
	titleLogo_->Update();
	skydome_->Update(sceneManager_->GetCameraManager());

	cube_.center = camera->GetTranslate() + Vector3(0.0f, -1.0f, 10.0f);

	// キューブの上下動（sinf波）
	cubeWaveTime += 0.05f;
	float baseY = 1.0f;
	float amplitude = 0.5f;
	cube_.center.y = baseY + amplitude * sinf(cubeWaveTime);

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

void TitleScene::OnEnterExit()
{
}

void TitleScene::OnUpdateExit()
{
	transitionEffect_.Update();

	if (transitionEffect_.GetState() == TransitionState::Done)
	{
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
	LineManager::GetInstance()->DrawGrid(
		600.0f,
		5.0f,
		VectorColorCodes::DarkGray
	);

	LineManager::GetInstance()->DrawOBB(
		cube_,
		VectorColorCodes::Cyan
	);

	skydome_->Draw();
}

void TitleScene::Draw2D()
{
	titleLogo_->Draw();
	fontSprite_->Draw();
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