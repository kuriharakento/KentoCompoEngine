#include "GamePlayScene.h"

// audio
#include "audio/Audio.h"
// scene
#include "engine/scene/manager/SceneManager.h"
// editor
#include "externals/imgui/imgui.h"
// input
#include "input/Input.h"
// math
#include "math/VectorColorCodes.h"
// graphics
#include "manager/graphics/LineManager.h"
#include "manager/effect/PostProcessManager.h"
// app
#include "application/GameObject/component/collision/CollisionManager.h"
// components
#include "application/combo/ComboManager.h"
#include "application/GameObject/component/action/PistolComponent.h"
#include "time/TimeManager.h"
#include "time/TimerManager.h"
#include "application/effect/BulletTrailManager.h"
#include "effects/particle/ParticleManager.h"

void GamePlayScene::Initialize()
{
	Audio::GetInstance()->LoadWave("game_bgm", "bgm/game.wav", SoundGroup::BGM);
	Audio::GetInstance()->PlayWave("game_bgm", true);
	Audio::GetInstance()->SetVolume("game_bgm", 0.2f);

	// ディレクショナルライトの調整（斜め下向き）
	DirectionalLight dirLight = sceneManager_->GetLightManager()->GetDirectionalLight();
	dirLight.direction = { -0.4f, -1.0f, 1.0f };
	dirLight.intensity = 0.3f;
	sceneManager_->GetLightManager()->SetDirectionalLight(dirLight);

	sceneManager_->GetCameraManager()->GetActiveCamera()->SetTranslate(cameraInitialPosition_);
	sceneManager_->GetCameraManager()->GetActiveCamera()->SetRotate(cameraInitialRotation_);

	skydome_ = std::make_unique<Object3d>();
	skydome_->Initialize(sceneManager_->GetObject3dCommon());
	skydome_->SetModel("skydome");
	skydome_->SetLightManager(sceneManager_->GetLightManager());
	skydome_->SetEnableLighting(true);
	skydome_->SetDirectionalLightIntensity(0.5f);
	skydome_->SetDirectionalLightDirection({ 0.0f, -1.0f, 0.0f });
	skydome_->SetScale({ 0.5f, 0.5f, 0.5f });
	RegisterObject(skydome_.get());

	// UVスケールで地形テクスチャをタイル状に繰り返し
	ground_ = std::make_unique<Object3d>();
	ground_->Initialize(sceneManager_->GetObject3dCommon());
	ground_->SetModel("terrain");
	ground_->SetLightManager(sceneManager_->GetLightManager());
	ground_->SetEnableLighting(true);
	ground_->GetModel()->SetUVScale(Vector3(10.0f, 10.0f, 1.0f));
	//RegisterObject(ground_.get());

	CollisionManager::GetInstance()->Initialize();

	ComboManager::GetInstance().Initialize(sceneManager_->GetSpriteCommon());
	ComboManager::GetInstance().Reset();

	stageManager_ = std::make_unique<StageManager>();
	stageManager_->Initialize(
		sceneManager_->GetObject3dCommon(),
		sceneManager_->GetSpriteCommon(),
		sceneManager_->GetLightManager(),
		sceneManager_->GetCameraManager()
	);
	stageManager_->LoadStage("stage_2");

	minimap_ = std::make_unique<Minimap>();
	minimap_->Initialize(sceneManager_->GetSpriteCommon(), stageManager_.get());

	splineCamera_ = std::make_unique<SplineCamera>();
	splineCamera_->Initialize(sceneManager_->GetCameraManager()->GetActiveCamera());
	splineCamera_->LoadJson("spline.json");
	splineCamera_->Start(0.001f, false);
	splineCamera_->SetTarget(&stageManager_->GetPlayer()->GetPosition());

	topDownCamera_ = std::make_unique<TopDownCamera>();
	topDownCamera_->Initialize(sceneManager_->GetCameraManager()->GetActiveCamera());
	topDownCamera_->SetOffset({ -45.0f, 0.0f, -28.0f });
	topDownCamera_->SetPitch(0.7f);
	topDownCamera_->SetYaw(1.0f);
	topDownCamera_->SetHeight(43.0f);

	orbitCamera_ = std::make_unique<OrbitCameraWork>();
	orbitCamera_->Initialize(sceneManager_->GetCameraManager()->GetActiveCamera());

	carnageMode_ = std::make_unique<CarnageMode>(stageManager_->GetPlayer());

	transitionEffect_.Initialize(
		sceneManager_->GetSpriteCommon(),
		"./Resources/black.png",
		22, 16,
		WinApp::kClientWidth, WinApp::kClientHeight
	);
	transitionEffect_.SetEaseType(SceneTransitionEase::InSine);
	transitionEffect_.SetFadeType(FadeType::FadeOut);
	transitionEffect_.SetMode(TransitionMode::RightBottomToLeftTop);

	playerDeathEffect_.Initialize(
		stageManager_->GetPlayer()
	);

	cinematicLetterbox_.Initialize(
		sceneManager_->GetSpriteCommon(),
		"./Resources/black.png",
		WinApp::kClientWidth, WinApp::kClientHeight
	);

	reticle_ = std::make_unique<Cursor>();
	reticle_->Initialize(
		sceneManager_->GetSpriteCommon(),
		"./Resources/UI/reticle.png"
	);

	BulletTrailManager::GetInstance().Initialize();

	controlsGuide_ = std::make_unique<ControlsGuide>();
	controlsGuide_->Initialize(sceneManager_->GetSpriteCommon(), "luna");
	controlsGuide_->SetText("WASD: Move\nShoot: Left Click\nDodge: Space\n");
	controlsGuide_->SetPosition({ 30.0f, 30.0f });
	controlsGuide_->SetScale(0.3f);
	controlsGuide_->SetVisible(false);

	StartState(SceneState::Enter);
	gameClear_ = false;
	gameOver_ = false;
}

void GamePlayScene::Finalize()
{
	ClearObjects();
	Audio::GetInstance()->StopWave("game_bgm");

	BulletTrailManager::GetInstance().Clear();

	CollisionManager::GetInstance()->Finalize();
}

// ================================================================
// 状態フック実装
// ================================================================

// ==================================================
// Enter状態（シーン開始・フェードイン演出）
// ==================================================
void GamePlayScene::OnEnterEnter()
{
	transitionEffect_.Start(
		1.5f,
		VectorColorCodes::Black,
		VectorColorCodes::Red
	);
}

void GamePlayScene::OnUpdateEnter()
{
	transitionEffect_.Update();

	if (transitionEffect_.GetState() == TransitionState::Done)
	{
		ChangeState(SceneState::Intro);
	}
}

void GamePlayScene::OnExitEnter()
{
}

// ==================================================
// Intro状態（ゲーム開始前のイントロ演出）
// ==================================================
void GamePlayScene::OnEnterIntro()
{
	introElapsed_ = 0.0f;
}

void GamePlayScene::OnUpdateIntro()
{
	float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;

	introElapsed_ += deltaTime;
	float t = introElapsed_ / introDuration_;
	if (t > 1.0f) t = 1.0f;
	// イージング関数で自然な加減速を実現
	float eased = EaseInOutCirc(t);

	auto activeCamera = sceneManager_->GetCameraManager()->GetActiveCamera();

	// イントロ演出：カメラをプレイヤー位置へ滑らかに移動
	Vector3 targetPos = stageManager_->GetPlayer()->GetPosition() + Vector3{ 0.0f, topDownCamera_->GetHeight(), 0.0f };
	Vector3 targetRot = { topDownCamera_->GetPitch(), topDownCamera_->GetYaw(), 0.0f };

	Vector3 nextPos = MathUtils::Lerp(cameraInitialPosition_, targetPos, eased);
	Vector3 nextRot = {
		LerpAngle(cameraInitialRotation_.x, targetRot.x, eased),
		LerpAngle(cameraInitialRotation_.y, targetRot.y, eased),
		LerpAngle(cameraInitialRotation_.z, targetRot.z, eased)
	};

	activeCamera->SetTranslate(nextPos + topDownCamera_->GetOffset());
	activeCamera->SetRotate(nextRot);

	if (t >= 1.0f)
	{
		activeCamera->SetTranslate(targetPos + topDownCamera_->GetOffset());
		activeCamera->SetRotate(targetRot);
		ChangeState(SceneState::Playing);
	}
}

// ==================================================
// Playing状態（メインゲームプレイ）
// ==================================================
void GamePlayScene::OnEnterPlaying()
{
	topDownCamera_->Start(
		43.0f,
		&stageManager_->GetPlayer()->GetPosition()
	);

	// 操作ガイド表示
	controlsGuide_->SetVisible(true);
}

void GamePlayScene::OnUpdatePlaying()
{
	// ゲーム終了条件の判定（早期リターンでパフォーマンス向上）
	if (stageManager_->IsStageCleared())
	{
		gameClear_ = true;
		ChangeState(SceneState::End);
		return;
	}

	if (!stageManager_->GetPlayer()->IsAlive())
	{
		gameOver_ = true;
		ChangeState(SceneState::End);
		return;
	}

	CollisionManager::GetInstance()->UpdatePreviousPositions();

	topDownCamera_->Update();

	minimap_->Update();

	reticle_->Update();

	stageManager_->Update();

	skydome_->Update(sceneManager_->GetCameraManager());
	ground_->Update(sceneManager_->GetCameraManager());

	CollisionManager::GetInstance()->CheckCollisions();

	ComboManager::GetInstance().Update();

	carnageMode_->Update();
}

void GamePlayScene::OnExitPlaying()
{
}

// ==================================================
// End状態（ゲーム終了演出：クリア/ゲームオーバー）
// ==================================================
void GamePlayScene::OnEnterEnd()
{
	if (gameOver_)
	{
		auto* ppm = sceneManager_->GetPostProcessManager();
		ppm->crtEffect_->SetEnabled(true);
		ppm->crtEffect_->SetCrtEnabled(true);
		ppm->crtEffect_->SetChromaticAberrationEnabled(true);

		playerDeathEffect_.Play(1.5f);
	}
	else if (gameClear_)
	{
		cinematicLetterbox_.Show(1.0f);

		auto timer = std::make_unique<Timer>("GameClearToExitTimer", 2.0f, DeltaTimeType::RealDeltaTime);

		timer->SetOnFinish([this]() {
			ChangeState(SceneState::Exit);
						   });

		TimerManager::GetInstance().AddTimer(std::move(timer));	

		float playerYaw = stageManager_->GetPlayer()->GetRotation().y;

		Vector3 forward = { std::sin(playerYaw), 0.0f, std::cos(playerYaw) };

		// orbit の角度は (cos, sin) = (x, z) の順なので atan2(z, x) を使う
		float baseOrbitAngle = std::atan2(forward.z, forward.x);

		const float offsetDeg = -30.0f;
		float offsetRad = offsetDeg * std::numbers::pi_v<float> / 180.0f;

		float initialAngle = MathUtils::NormalizeAngleRad(baseOrbitAngle + offsetRad);

		orbitCamera_->Start(
			&stageManager_->GetPlayer()->GetPosition(),
			15.0f,
			0.5f,
			initialAngle,
			DeltaTimeType::RealDeltaTime
		);
	}
}

void GamePlayScene::OnUpdateEnd()
{
	if (gameOver_)
	{
		playerDeathEffect_.Update();

		gameOverEffectElapsed_ += TimeManager::GetInstance().GetGameContext().deltaTime;

		// 色収差を振幅で揺らす（指数減衰で自然に収束）
		const float frequencyHz = 4.0f;
		const float maxOscAmp = 35.0f;
		const float decayRate = 2.8f;

		float envelope = maxOscAmp * std::exp(-decayRate * gameOverEffectElapsed_);
		if (envelope < 0.001f)
		{
			envelope = 0.0f;
		}

		const float twoPi = std::numbers::pi_v<float> *2.0f;
		float oscill = std::sinf(gameOverEffectElapsed_ * frequencyHz * twoPi) * envelope;

		auto* ppm = sceneManager_->GetPostProcessManager();
		ppm->crtEffect_->SetChromaticAberrationOffset(oscill);

		if (playerDeathEffect_.IsFinished())
		{
			ChangeState(SceneState::Exit);
		}
	}
	else if (gameClear_)
	{
		cinematicLetterbox_.Update();
		orbitCamera_->Update();
	}
}

void GamePlayScene::OnExitEnd()
{
}

// ==================================================
// Exit状態（シーン退場・次シーンへの遷移）
// ==================================================
void GamePlayScene::OnEnterExit()
{
	transitionEffect_.SetEaseType(SceneTransitionEase::InSine);
	transitionEffect_.SetFadeType(FadeType::FadeIn);
	transitionEffect_.SetMode(TransitionMode::EdgesToCenter);
	transitionEffect_.Start(
		2.0f,
		VectorColorCodes::Black,
		VectorColorCodes::Red
	);
}

void GamePlayScene::OnUpdateExit()
{
	transitionEffect_.Update();

	if (transitionEffect_.GetState() == TransitionState::Done)
	{
		if (gameClear_)
		{
			sceneManager_->ChangeScene(SceneNames::GameClear);
		}
		else if (gameOver_)
		{
			sceneManager_->ChangeScene(SceneNames::GameOver);
		}
	}
}

void GamePlayScene::OnExitExit()
{
	auto* ppm = sceneManager_->GetPostProcessManager();
	ppm->crtEffect_->SetEnabled(false);
	ppm->crtEffect_->SetCrtEnabled(false);
	ppm->crtEffect_->SetChromaticAberrationEnabled(false);
}

// ==================================================
// 全状態共通の更新処理
// ==================================================
void GamePlayScene::CommonUpdate()
{
	cinematicLetterbox_.Update();
	skydome_->Update(sceneManager_->GetCameraManager());
	ground_->Update(sceneManager_->GetCameraManager());
	stageManager_->UpdateTransforms(sceneManager_->GetCameraManager());
}

// ================================================================
// 描画処理
// ================================================================

void GamePlayScene::Draw3D()
{
	BaseScene::Draw3D();

	stageManager_->Draw3D();

	splineCamera_->DrawSplineLine();
}

void GamePlayScene::DrawShadow()
{
	BaseScene::DrawShadow();

	stageManager_->DrawShadow();
}

void GamePlayScene::Draw2D()
{
	stageManager_->Draw2D();

	minimap_->Draw();

	reticle_->Draw();

	ComboManager::GetInstance().Draw();

	cinematicLetterbox_.Draw();

	controlsGuide_->Draw();

	transitionEffect_.Draw();
}

void GamePlayScene::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Begin("GameScene");

	if (ImGui::Button("Clear"))
	{
		gameClear_ = true;
		ChangeState(SceneState::End);
	}
	if (ImGui::Button("GameOver"))
	{
		gameOver_ = true;
		ChangeState(SceneState::End);
	}

	static bool useDebugCamera = false;
	static bool useSplineCamera = false;
	static bool loopSpline = false;
	static float speed = 0.001f;
	static bool useTopDownCamera = false;
	if (ImGui::CollapsingHeader("Camera Work", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::SeparatorText("Debug Camera");
		ImGui::Checkbox("Use Debug Camera", &useDebugCamera);
		ImGui::SeparatorText("Spline Camera");
		ImGui::DragFloat("Spline Speed", &speed, 0.001f, 0.0f, 0.1f);
		if (ImGui::Checkbox("Loop Spline", &loopSpline))
		{
			splineCamera_->Start(speed, loopSpline);
		}
		ImGui::Checkbox("Use Spline Camera", &useSplineCamera);
		ImGui::SeparatorText("Top Down Camera");
		ImGui::Checkbox("Use Top Down Camera", &useTopDownCamera);
	}
	if (useSplineCamera)
	{
		splineCamera_->Update();
	}
	if (useTopDownCamera)
	{
		topDownCamera_->Update();
	}

#pragma region PostProcess
	ImGui::SeparatorText("PostProcess");
	if (ImGui::CollapsingHeader("GrayScale"))
	{
		static bool isGrayScale = false;
		if (ImGui::Checkbox("enable", &isGrayScale))
		{
			sceneManager_->GetPostProcessManager()->grayscaleEffect_->SetEnabled(isGrayScale);
		}
		float intensity = sceneManager_->GetPostProcessManager()->grayscaleEffect_->GetIntensity();
		ImGui::DragFloat("GrayScale Intensity", &intensity, 0.01f, 0.0f, 1.0f);
		sceneManager_->GetPostProcessManager()->grayscaleEffect_->SetIntensity(intensity);
	}
	if (ImGui::CollapsingHeader("Vignette"))
	{
		static bool isVignette = false;
		if (ImGui::Checkbox("enable", &isVignette))
		{
			sceneManager_->GetPostProcessManager()->vignetteEffect_->SetEnabled(isVignette);
		}
		float intensity = sceneManager_->GetPostProcessManager()->vignetteEffect_->GetIntensity();
		ImGui::DragFloat("Vignette Intensity", &intensity, 0.01f, 0.0f, 1.0f);
		sceneManager_->GetPostProcessManager()->vignetteEffect_->SetIntensity(intensity);
		float radius = sceneManager_->GetPostProcessManager()->vignetteEffect_->GetRadius();
		ImGui::DragFloat("Vignette Radius", &radius, 0.01f, 0.0f, 1.0f);
		sceneManager_->GetPostProcessManager()->vignetteEffect_->SetRadius(radius);
		float softness = sceneManager_->GetPostProcessManager()->vignetteEffect_->GetSoftness();
		ImGui::DragFloat("Vignette Softness", &softness, 0.01f, 0.0f, 1.0f);
		sceneManager_->GetPostProcessManager()->vignetteEffect_->SetSoftness(softness);
		Vector3 color = sceneManager_->GetPostProcessManager()->vignetteEffect_->GetColor();
		ImGui::ColorEdit3("Vignette Color", &color.x);
		sceneManager_->GetPostProcessManager()->vignetteEffect_->SetColor(color);
	}
	if (ImGui::CollapsingHeader("Noise"))
	{
		static bool isNoise = false;
		if (ImGui::Checkbox("enable", &isNoise))
		{
			sceneManager_->GetPostProcessManager()->noiseEffect_->SetEnabled(isNoise);
		}
		float intensity = sceneManager_->GetPostProcessManager()->noiseEffect_->GetIntensity();
		ImGui::DragFloat("Noise Intensity", &intensity, 0.01f, 0.0f, 1.0f);
		sceneManager_->GetPostProcessManager()->noiseEffect_->SetIntensity(intensity);
		float time = sceneManager_->GetPostProcessManager()->noiseEffect_->GetTime();
		ImGui::DragFloat("Noise Time", &time, 0.01f, 0.0f, 10.0f);
		sceneManager_->GetPostProcessManager()->noiseEffect_->SetTime(time);
		float grainSize = sceneManager_->GetPostProcessManager()->noiseEffect_->GetGrainSize();
		ImGui::DragFloat("Noise Grain Size", &grainSize, 0.01f, 0.0f, 10.0f);
		sceneManager_->GetPostProcessManager()->noiseEffect_->SetGrainSize(grainSize);
		float luminanceAffect = sceneManager_->GetPostProcessManager()->noiseEffect_->GetLuminanceAffect();
		ImGui::DragFloat("Noise Luminance Affect", &luminanceAffect, 0.01f, 0.0f, 1.0f);
		sceneManager_->GetPostProcessManager()->noiseEffect_->SetLuminanceAffect(luminanceAffect);
	}
	if (ImGui::CollapsingHeader("CRT"))
	{
		static  bool isEnabled = false;
		static bool isCrt = false; // CRTエフェクトの有効/無効
		static bool isScanline = false;
		static bool isDistortion = false;
		static bool isChromAberration = false;

		if (ImGui::Checkbox("enable", &isEnabled))
		{
			sceneManager_->GetPostProcessManager()->crtEffect_->SetEnabled(isEnabled);
		}
		if (ImGui::Checkbox("Crt", &isCrt))
		{
			sceneManager_->GetPostProcessManager()->crtEffect_->SetCrtEnabled(isCrt);
		}
		ImGui::SameLine();
		if (ImGui::Checkbox("Scanline", &isScanline))
		{
			sceneManager_->GetPostProcessManager()->crtEffect_->SetScanlineEnabled(isScanline);
		}
		ImGui::SameLine();
		if (ImGui::Checkbox("Distortion", &isDistortion))
		{
			sceneManager_->GetPostProcessManager()->crtEffect_->SetDistortionEnabled(isDistortion);
		}
		ImGui::SameLine();
		if (ImGui::Checkbox("ChromAberration", &isChromAberration))
		{
			sceneManager_->GetPostProcessManager()->crtEffect_->SetChromaticAberrationEnabled(isChromAberration);
		}
		float scanlineIntensity = sceneManager_->GetPostProcessManager()->crtEffect_->GetScanlineIntensity();
		ImGui::DragFloat("Scanline Intensity", &scanlineIntensity, 0.01f, 0.0f, 1.0f);
		sceneManager_->GetPostProcessManager()->crtEffect_->SetScanlineIntensity(scanlineIntensity);
		float scanlineCount = sceneManager_->GetPostProcessManager()->crtEffect_->GetScanlineCount();
		ImGui::DragFloat("Scanline Count", &scanlineCount, 10.0f, 0.0f, 1000.0f);
		sceneManager_->GetPostProcessManager()->crtEffect_->SetScanlineCount(scanlineCount);
		float distortionStrength = sceneManager_->GetPostProcessManager()->crtEffect_->GetDistortionStrength();
		ImGui::DragFloat("Distortion Strength", &distortionStrength, 0.01f, 0.0f, 10.0f);
		sceneManager_->GetPostProcessManager()->crtEffect_->SetDistortionStrength(distortionStrength);
		float chromAberrationOffset = sceneManager_->GetPostProcessManager()->crtEffect_->GetChromaticAberrationOffset();
		ImGui::DragFloat("Chromatic Aberration Offset", &chromAberrationOffset, 0.01f, 0.0f, 10.0f);
		sceneManager_->GetPostProcessManager()->crtEffect_->SetChromaticAberrationOffset(chromAberrationOffset);
	}
	// bloom
	if (ImGui::CollapsingHeader("Bloom"))
	{
		static bool isBloom = true;
		if (ImGui::Checkbox("enable", &isBloom))
		{
			sceneManager_->GetPostProcessManager()->bloomEffect_->SetEnabled(isBloom);
		}
		float threshold = sceneManager_->GetPostProcessManager()->bloomEffect_->GetThreshold();
		ImGui::DragFloat("Bloom Threshold", &threshold, 0.01f, 0.0f, 1.0f);
		sceneManager_->GetPostProcessManager()->bloomEffect_->SetThreshold(threshold);
		float intensity = sceneManager_->GetPostProcessManager()->bloomEffect_->GetIntensity();
		ImGui::DragFloat("Bloom Intensity", &intensity, 0.01f, 0.0f, 10.0f);
		sceneManager_->GetPostProcessManager()->bloomEffect_->SetIntensity(intensity);
		float radius = sceneManager_->GetPostProcessManager()->bloomEffect_->GetRadius();
		ImGui::DragFloat("Bloom Radius", &radius, 0.01f, 0.0f, 10.0f);
		sceneManager_->GetPostProcessManager()->bloomEffect_->SetRadius(radius);
		float thresholdknee = sceneManager_->GetPostProcessManager()->bloomEffect_->GetThresholdKnee();
		ImGui::DragFloat("Bloom ThresholdKnee", &thresholdknee, 0.01f, 0.0f, 1.0f);
		sceneManager_->GetPostProcessManager()->bloomEffect_->SetThresholdKnee(thresholdknee);
		float mix = sceneManager_->GetPostProcessManager()->bloomEffect_->GetBloomMix();
		ImGui::DragFloat("Bloom Mix", &mix, 0.01f, 0.0f, 1.0f);
		sceneManager_->GetPostProcessManager()->bloomEffect_->SetBloomMix(mix);
	}
	// --- BrightPass ---
	if (ImGui::CollapsingHeader("BrightPass"))
	{
		auto* ppm = sceneManager_->GetPostProcessManager();
		ImGui::DragFloat("Threshold", &ppm->brightPassParams_.threshold, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("Intensity", &ppm->brightPassParams_.intensity, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("Knee", &ppm->brightPassParams_.knee, 0.01f, 0.0f, 1.0f);
	}

	// --- Blur ---
	if (ImGui::CollapsingHeader("Blur"))
	{
		auto* ppm = sceneManager_->GetPostProcessManager();
		ImGui::DragFloat2("Blur Direction", &ppm->blurParams_.blurDirection.x, 0.01f, -1.0f, 1.0f);
		ImGui::DragFloat("Radius", &ppm->blurParams_.radius, 0.01f, 0.0f, 10.0f);
	}
#pragma endregion

	ImGui::End();
#endif
}