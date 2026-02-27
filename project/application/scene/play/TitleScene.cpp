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
#include <effects/particle/ParticleManager.h>
#include "effects/particle/ParticleEffect.h"

void TitleScene::Initialize()
{
	Audio::GetInstance()->LoadWave("title_bgm", "bgm/title.wav", SoundGroup::BGM);
	Audio::GetInstance()->PlayWave("title_bgm", true);
	Audio::GetInstance()->SetVolume("title_bgm", kBgmVolume);
	Audio::GetInstance()->LoadWave("start_se", "se/tap.wav", SoundGroup::SE);

	// パーティクルをJsonから読み込み
	ParticleManager::GetInstance()->LoadEffectDefinition("title_particle", "./Resources/json/particle/title_particle.json");
	auto particleEffect = ParticleManager::GetInstance()->Play("title_particle", Vector3());

	// ディレクショナルライトの調整（下向き）
	DirectionalLight dirLight = sceneManager_->GetLightManager()->GetDirectionalLight();
	dirLight.direction = { 0.0f, -1.0f, 0.0f };  // 下向き
	dirLight.intensity = kLightIntensity;
	sceneManager_->GetLightManager()->SetDirectionalLight(dirLight);

	sceneManager_->GetCameraManager()->GetActiveCamera()->SetTranslate(Vector3(0.0f, kCameraHeight, kCameraInitialZ));
	sceneManager_->GetCameraManager()->GetActiveCamera()->SetRotate(Vector3());

	titleLogo_ = std::make_unique<Sprite>();
	titleLogo_->Initialize(sceneManager_->GetSpriteCommon(), "./Resources/title_logo.png");
	titleLogo_->SetPosition({ kLogoPositionX, kLogoPositionY });
	titleLogo_->SetAnchorPoint({ 0.5f, 0.5f });  // 中心起点
	titleLogo_->SetSize({ kLogoWidth, kLogoHeight });

	skydome_ = std::make_unique<Object3d>();
	skydome_->Initialize(sceneManager_->GetObject3dCommon());
	skydome_->SetModel("skydome");
	skydome_->SetLightManager(sceneManager_->GetLightManager());
	skydome_->SetEnableLighting(true);
	skydome_->SetDirectionalLightIntensity(kSkydomeLightIntensity);
	skydome_->SetDirectionalLightDirection({ 0.0f, -1.0f, 0.0f });  // 真下向き
	skydome_->SetScale({ kSkydomeScale, kSkydomeScale, kSkydomeScale });
	skydome_->SetCastShadow(false);
	RegisterObject(skydome_.get());

	cube_.center = Vector3(0.0f, kCubeBaseY, kCubeDistanceFromCamera);
	cube_.size = Vector3(1.0f, 1.0f, 1.0f);  // 単位サイズ
	cube_.rotate = MakeRotateYMatrix(0.0f);

	transitionEffect_.Initialize(
		sceneManager_->GetSpriteCommon(),
		"./Resources/black.png",
		kTransitionGridX, kTransitionGridY,
		WinApp::kClientWidth, WinApp::kClientHeight
	);

	// 色収差エフェクトを有効化してレトロ風の雰囲気を演出
	sceneManager_->GetPostProcessManager()->crtEffect_->SetEnabled(true);
	sceneManager_->GetPostProcessManager()->crtEffect_->SetCrtEnabled(true);
	sceneManager_->GetPostProcessManager()->crtEffect_->SetChromaticAberrationEnabled(true);
	sceneManager_->GetPostProcessManager()->crtEffect_->SetChromaticAberrationOffset(kChromaticAberrationOffset);

	// フォントスプライトの初期化（テスト用）
	fontSprite_ = std::make_unique<FontSprite>();
	fontSprite_->Initialize(
		sceneManager_->GetSpriteCommon(),
		"luna"
	);
	fontSprite_->SetText("Click to Start");
	fontSprite_->SetPosition({ kFontPositionX, kFontPositionY });
	fontSprite_->SetScale(kFontScale);
	fontSprite_->SetColor(VectorColorCodes::Cyan);

	StartState(SceneState::Playing);
}

void TitleScene::Finalize()
{
	ClearObjects();
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

#ifdef _DEBUG
	// デバッグ時は右クリックで開始
	if (Input::GetInstance()->IsMouseButtonTriggered(2))
	{
		Audio::GetInstance()->PlayWave("start_se", false);

		transitionEffect_.SetEaseType(SceneTransitionEase::InSine);
		transitionEffect_.SetFadeType(FadeType::FadeIn);
		transitionEffect_.SetMode(TransitionMode::LeftTopToRightBottom);
		transitionEffect_.Start(
			kTransitionDuration,
			VectorColorCodes::Red,
			VectorColorCodes::Black
		);

		ChangeState(SceneState::Exit);
	}
#else
	// マウス左右のクリックで開始
	if (Input::GetInstance()->IsMouseButtonTriggered(0) || Input::GetInstance()->IsMouseButtonTriggered(2))
	{
		Audio::GetInstance()->PlayWave("start_se", false);

		transitionEffect_.SetEaseType(SceneTransitionEase::InSine);
		transitionEffect_.SetFadeType(FadeType::FadeIn);
		transitionEffect_.SetMode(TransitionMode::LeftTopToRightBottom);
		transitionEffect_.Start(
			kTransitionDuration,
			VectorColorCodes::Red,
			VectorColorCodes::Black
		);

		ChangeState(SceneState::Exit);
	}
#endif // _DEBUG

	auto camera = sceneManager_->GetCameraManager()->GetActiveCamera();
	camera->SetTranslate(camera->GetTranslate() + Vector3(0.0f, 0.0f, kCameraMoveSpeed));
	if (camera->GetTranslate().z >= kCameraResetZ)
	{
		camera->SetTranslate({ 0.0f, kCameraHeight, kCameraInitialZ });
	}

	auto particleEffect = ParticleManager::GetInstance()->GetEffect("title_particle");
	particleEffect->SetPosition(cube_.center);

	transitionEffect_.Update();
	titleLogo_->Update();
	skydome_->Update(sceneManager_->GetCameraManager());

	cube_.center = camera->GetTranslate() + Vector3(0.0f, kCubeOffsetY, kCubeDistanceFromCamera);

	// キューブの上下動（sinf波）
	cubeWaveTime += kCubeWaveSpeed;
	cube_.center.y = kCubeBaseY + kCubeAmplitude * sinf(cubeWaveTime);

	cubeRotateY += kCubeRotateSpeed;
	if (cubeRotateY >= kCubeMaxRotateY)
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
		kGridSize,
		kGridSpacing,
		VectorColorCodes::DarkGray
	);

	LineManager::GetInstance()->DrawOBB(
		cube_,
		VectorColorCodes::Cyan
	);

	BaseScene::Draw3D();
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