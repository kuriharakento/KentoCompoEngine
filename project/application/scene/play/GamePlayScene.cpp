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
#include "engine/gameobject/component/collision/CollisionManager.h"
// components
#include "application/combo/ComboManager.h"
#include "application/GameObject/component/action/PistolComponent.h"
#include "time/TimeManager.h"
#include "time/TimerManager.h"
#include "application/effect/BulletTrailManager.h"
#include "effects/particle/ParticleManager.h"
#include "application/UI/PoseMenu.h"

// ECS Integration
#include "engine/ecs/system/HierarchySystem.h"
#include "application/ecs/systems/EnemyBehaviorSystem.h"
#include "engine/ecs/system/InstancedRenderSystem.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/HierarchyComponent.h"
#include "engine/ecs/components/TagComponent.h"
#include "engine/ecs/components/MovementComponent.h"
#include "engine/ecs/components/ColliderComponent.h"
#include "engine/ecs/components/CollisionLayerComponent.h"
#include "application/ecs/components/PlayerComponent.h"
#include "application/ecs/components/EnemyStateComponent.h"
#include "application/ecs/components/StatusComponent.h"
#include "engine/ecs/components/EnemyAIComponent.h"
#include "engine/ecs/components/InstancedRenderComponent.h"
#include "engine/ecs/components/CollisionResponseComponent.h"

// Systems
#include "engine/ecs/system/HierarchySystem.h"
#include "engine/ecs/system/MovementSystem.h"
#include "engine/ecs/system/CollisionSystem.h"
#include "engine/ecs/system/PlayerSystem.h"
#include "engine/ecs/system/EcsStatusSystem.h"
#include "engine/ecs/system/InstancedRenderSystem.h"
#include "application/ecs/systems/EnemyBehaviorSystem.h"

void GamePlayScene::Initialize()
{
	Audio::GetInstance()->LoadWave("game_bgm", "bgm/game.wav", SoundGroup::BGM);
	Audio::GetInstance()->PlayWave("game_bgm", true);
	Audio::GetInstance()->SetVolume("game_bgm", kBgmVolume);
	Audio::GetInstance()->LoadWave("fire_se", "se/fire.wav", SoundGroup::SE);
	Audio::GetInstance()->SetVolume("fire_se", KSeVolume);
	Audio::GetInstance()->LoadWave("enemy_kill", "se/enemy_kill.wav", SoundGroup::SE);
	Audio::GetInstance()->SetVolume("enemy_kill", KSeVolume);

	// パーティクルの読み込み
	ParticleManager::GetInstance()->Load("hit_effect", "./Resources/json/particle/hit_effect.json");
	ParticleManager::GetInstance()->Load("hit_effect2", "./Resources/json/particle/hit_effect_ver2.json");
	ParticleManager::GetInstance()->Load("dodge_effect", "./Resources/json/particle/dodge.json");
	ParticleManager::GetInstance()->Load("bulletTime_finish_effect", "./Resources/json/particle/bulletTime_finish_effect.json");
	ParticleManager::GetInstance()->Load("enemy_spawn_effect", "./Resources/json/particle/enemy_spawn_effect.json");

	// ディレクショナルライトの調整（斜め下向き）
	DirectionalLight dirLight = sceneManager_->GetLightManager()->GetDirectionalLight();
	dirLight.direction = kLightDirection;
	dirLight.intensity = kLightIntensity;
	sceneManager_->GetLightManager()->SetDirectionalLight(dirLight);

	sceneManager_->GetCameraManager()->GetActiveCamera()->SetTranslate(cameraInitialPosition_);
	sceneManager_->GetCameraManager()->GetActiveCamera()->SetRotate(cameraInitialRotation_);

	skydome_ = std::make_unique<Object3d>();
	skydome_->Initialize(sceneManager_->GetObject3dCommon());
	skydome_->SetModel("skydome");
	skydome_->SetLightManager(sceneManager_->GetLightManager());
	skydome_->SetEnableLighting(true);
	skydome_->SetDirectionalLightIntensity(kSkydomeLightIntensity);
	skydome_->SetDirectionalLightDirection({ 0.0f, -1.0f, 0.0f });  // 真下向き
	skydome_->SetScale({ kSkydomeScale, kSkydomeScale, kSkydomeScale });
	RegisterObject(skydome_.get());

	// UVスケールで地形テクスチャをタイル状に繰り返し
	ground_ = std::make_unique<Object3d>();
	ground_->Initialize(sceneManager_->GetObject3dCommon());
	ground_->SetModel("terrain");
	ground_->SetLightManager(sceneManager_->GetLightManager());
	ground_->SetEnableLighting(true);
	constexpr float kGroundUVTile = 10.0f;  // 地面テクスチャのタイル繰り返し数
	ground_->GetModel()->SetUVScale(Vector3(kGroundUVTile, kGroundUVTile, 1.0f));

	CollisionManager::GetInstance()->Initialize();

	ComboManager::GetInstance().Initialize(sceneManager_->GetSpriteCommon());
	ComboManager::GetInstance().Reset();

	registry_ = std::make_unique<Registry>();
	registry_->Initialize(10000);
	registry_->RegisterComponent<TransformComponent>(10000);
	registry_->RegisterComponent<EnemyStateComponent>(10000);
	registry_->RegisterComponent<InstancedRenderComponent>(10000);
	registry_->RegisterComponent<EnemyAIComponent>(10000);
	registry_->RegisterComponent<HierarchyComponent>(10000);
	registry_->RegisterComponent<TagComponent>(10000);
	registry_->RegisterComponent<MovementComponent>(10000);
	registry_->RegisterComponent<ColliderComponent>(10000);
	registry_->RegisterComponent<CollisionLayerComponent>(10000);
	registry_->RegisterComponent<PlayerComponent>(1);
	registry_->RegisterComponent<ecs::StatusComponent>(10000);
	registry_->RegisterComponent<ObstacleComponent>(10000);
	registry_->RegisterComponent<CollisionResponseComponent>(10000);

	systemManager_ = std::make_unique<SystemManager>();
	systemManager_->AddSystem(std::make_shared<HierarchySystem>());
	systemManager_->AddSystem(std::make_shared<MovementSystem>());
	systemManager_->AddSystem(std::make_shared<EcsStatusSystem>()); // Changed from StatusSystem
	systemManager_->AddSystem(std::make_shared<CollisionSystem>());

	auto playerSystem = std::make_shared<PlayerSystem>();
	playerSystem->SetCameraManager(sceneManager_->GetCameraManager());
	systemManager_->AddSystem(playerSystem);

	systemManager_->AddSystem(std::make_shared<EnemyBehaviorSystem>());
	systemManager_->AddSystem(std::make_shared<InstancedRenderSystem>());

	stageManager_ = std::make_unique<StageManager>();
	stageManager_->Initialize(
		registry_.get(),
		systemManager_.get(),
		sceneManager_->GetObject3dCommon(),
		sceneManager_->GetSpriteCommon(),
		sceneManager_->GetLightManager(),
		sceneManager_->GetCameraManager(),
		sceneManager_->GetShadowMapManager(),
		sceneManager_->GetPostProcessManager()
	);
	stageManager_->LoadStage("stage_2");

	minimap_ = std::make_unique<Minimap>();
	minimap_->Initialize(sceneManager_->GetSpriteCommon(), stageManager_.get());

	splineCamera_ = std::make_unique<SplineCamera>();
	splineCamera_->Initialize(sceneManager_->GetCameraManager()->GetActiveCamera());
	splineCamera_->LoadJson("spline.json");
	splineCamera_->Start(kSplineCameraSpeed, false);
	splineCamera_->SetTarget(stageManager_->GetPlayerPositionPtr());

	topDownCamera_ = std::make_unique<TopDownCamera>();
	topDownCamera_->Initialize(sceneManager_->GetCameraManager()->GetActiveCamera());
	// トップダウンカメラオフセット（シーン調整で決定した値）
	constexpr float kTopDownOffsetX = -45.0f;
	constexpr float kTopDownOffsetZ = -28.0f;
	topDownCamera_->SetOffset({ kTopDownOffsetX, 0.0f, kTopDownOffsetZ });
	topDownCamera_->SetPitch(kTopDownCameraPitch);
	topDownCamera_->SetYaw(kTopDownCameraYaw);
	topDownCamera_->SetHeight(kTopDownCameraHeight);

	orbitCamera_ = std::make_unique<OrbitCameraWork>();
	orbitCamera_->Initialize(sceneManager_->GetCameraManager()->GetActiveCamera());

	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize(sceneManager_->GetCameraManager()->GetActiveCamera());

	carnageMode_ = std::make_unique<CarnageMode>(registry_.get(), stageManager_->GetPlayerEntity());

	transitionEffect_.Initialize(
		sceneManager_->GetSpriteCommon(),
		"./Resources/black.png",
		kTransitionGridX, kTransitionGridY,
		WinApp::kClientWidth, WinApp::kClientHeight
	);
	transitionEffect_.SetEaseType(SceneTransitionEase::InSine);
	transitionEffect_.SetFadeType(FadeType::FadeOut);
	transitionEffect_.SetMode(TransitionMode::RightBottomToLeftTop);

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
	constexpr float kControlsGuidePosX = 30.0f;
	constexpr float kControlsGuidePosY = 30.0f;
	controlsGuide_->SetPosition({ kControlsGuidePosX, kControlsGuidePosY });
	controlsGuide_->SetScale(kControlsGuideScale);
	controlsGuide_->SetVisible(false);

	// ポーズメニュー初期化
	poseMenu_ = std::make_unique<PoseMenu>();
	poseMenu_->Initialize(sceneManager_->GetSpriteCommon());
	poseMenu_->SetOnRetryCallback([this]() {
		// リトライ：シーンを再読み込み
		sceneManager_->ChangeScene(SceneNames::GamePlay);
	});
	poseMenu_->SetOnExitCallback([this]() {
		// アプリケーションを終了
		PostQuitMessage(0);
	});

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

	// ポストエフェクトを無効化
	auto* ppm = sceneManager_->GetPostProcessManager();
	ppm->crtEffect_->SetEnabled(false);
	ppm->crtEffect_->SetCrtEnabled(false);
	ppm->crtEffect_->SetChromaticAberrationEnabled(false);
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
		kEnterTransitionDuration,
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
	Vector3 targetPos = stageManager_->GetPlayerPosition() + Vector3{ 0.0f, topDownCamera_->GetHeight(), 0.0f };
	Vector3 targetRot = { topDownCamera_->GetPitch(), topDownCamera_->GetYaw(), 0.0f };

	Vector3 nextPos = MathUtils::Lerp(cameraInitialPosition_, targetPos, eased);
	Vector3 nextRot = {
		LerpAngle(cameraInitialRotation_.x, targetRot.x, eased),
		LerpAngle(cameraInitialRotation_.y, targetRot.y, eased),
		LerpAngle(cameraInitialRotation_.z, targetRot.z, eased)
	};

	if (!isDebugCameraActive_)
	{
		activeCamera->SetTranslate(nextPos + topDownCamera_->GetOffset());
		activeCamera->SetRotate(nextRot);
	}

	if (t >= 1.0f)
	{
		if (!isDebugCameraActive_)
		{
			activeCamera->SetTranslate(targetPos + topDownCamera_->GetOffset());
			activeCamera->SetRotate(targetRot);
		}
		ChangeState(SceneState::Playing);
	}
}

// ==================================================
// Playing状態（メインゲームプレイ）
// ==================================================
void GamePlayScene::OnEnterPlaying()
{
	topDownCamera_->Start(
		kTopDownCameraHeight,
		stageManager_->GetPlayerPositionPtr()
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

	if (!stageManager_->IsPlayerAlive())
	{
		gameOver_ = true;
		ChangeState(SceneState::End);
		return;
	}

	CollisionManager::GetInstance()->UpdatePreviousPositions();

	if (!isDebugCameraActive_)
	{
		topDownCamera_->Update();
	}

	minimap_->Update();

	reticle_->Update();

	stageManager_->Update();

	if (registry_ && systemManager_) {
		systemManager_->Update(*registry_);
		registry_->FlushGarbageCollection();
	}

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
	}
	else if (gameClear_)
	{
		cinematicLetterbox_.Show(kLetterboxShowDuration);

		auto timer = std::make_unique<Timer>("GameClearToExitTimer", kClearToExitDelay, DeltaTimeType::RealDeltaTime);

		timer->SetOnFinish([this]() {
			ChangeState(SceneState::Exit);
						   });

		TimerManager::GetInstance().AddTimer(std::move(timer));	

		float playerYaw = stageManager_->GetPlayerRotation().y;

		Vector3 forward = { std::sin(playerYaw), 0.0f, std::cos(playerYaw) };

		// orbit の角度は (cos, sin) = (x, z) の順なので atan2(z, x) を使う
		float baseOrbitAngle = std::atan2(forward.z, forward.x);

		const float offsetDeg = kOrbitAngleOffsetDeg;
		float offsetRad = offsetDeg * std::numbers::pi_v<float> / 180.0f;

		float initialAngle = MathUtils::NormalizeAngleRad(baseOrbitAngle + offsetRad);

		orbitCamera_->Start(
			stageManager_->GetPlayerPositionPtr(),
			kOrbitCameraDistance,
			kOrbitCameraSpeed,
			initialAngle,
			DeltaTimeType::RealDeltaTime
		);
	}
}

void GamePlayScene::OnUpdateEnd()
{
	if (gameOver_)
	{
		gameOverEffectElapsed_ += TimeManager::GetInstance().GetGameContext().deltaTime;

		// 色収差を振幅で揺らす（指数減衰で自然に収束）
		float envelope = kGameOverMaxOscAmp * std::exp(-kGameOverDecayRate * gameOverEffectElapsed_);
		if (envelope < kGameOverEnvelopeThreshold)
		{
			envelope = 0.0f;
		}

		const float twoPi = std::numbers::pi_v<float> * 2.0f;
		float oscill = std::sinf(gameOverEffectElapsed_ * kGameOverFrequencyHz * twoPi) * envelope;

		auto* ppm = sceneManager_->GetPostProcessManager();
		ppm->crtEffect_->SetChromaticAberrationOffset(oscill);

		// NOTE:プレイヤーの死亡エフェクトを削除したので一時的にすぐ画面を切り替えるようにしてある
		ChangeState(SceneState::Exit);
	}
	else if (gameClear_)
	{
		cinematicLetterbox_.Update();
		if (!isDebugCameraActive_)
		{
			orbitCamera_->Update();
		}
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
		kExitTransitionDuration,
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
	// ポーズメニュー更新（全状態で動作）
	poseMenu_->Update();

	cinematicLetterbox_.Update();
	skydome_->Update(sceneManager_->GetCameraManager());
	ground_->Update(sceneManager_->GetCameraManager());
	stageManager_->UpdateTransforms(sceneManager_->GetCameraManager());

	if (isDebugCameraActive_)
	{
		debugCamera_->Update();
	}
}

// ================================================================
// 描画処理
// ================================================================

void GamePlayScene::Draw3D()
{
	BaseScene::Draw3D();

	stageManager_->Draw3D();

	// ECS Systemの描画 (コライダーのデバッグ表示など)
	systemManager_->Draw(*registry_, sceneManager_->GetCameraManager()->GetActiveCamera(), sceneManager_->GetLightManager(), sceneManager_->GetShadowMapManager());

	splineCamera_->DrawSplineLine();
}

void GamePlayScene::DrawShadow()
{
	BaseScene::DrawShadow();

	stageManager_->DrawShadow();
}

void GamePlayScene::Draw2D()
{
	// ステートがPlayの時のみ描画する
	if (GetCurrentState() == SceneState::Playing)
	{
		// 敵などのUI
		stageManager_->Draw2D();
		// ミニマップUI
		minimap_->Draw();
		// レティクルUI
		reticle_->Draw();
		// コンボUI
		ComboManager::GetInstance().Draw();
		// 操作UI
		controlsGuide_->Draw();
	}

	// ゲーム終了演出のレターボックスを描画
	cinematicLetterbox_.Draw();
	// ポーズメニュー
	poseMenu_->Draw();
	// シーン遷移の描画
	transitionEffect_.Draw();
}

void GamePlayScene::DrawImGui()
{
#ifdef USE_IMGUI
	// ポーズメニューUI
	poseMenu_->DrawImGui();

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

	bool prevDebugCamera = isDebugCameraActive_;
	static bool useSplineCamera = false;
	static bool loopSpline = false;
	static float speed = 0.001f;
	static bool useTopDownCamera = false;
	if (ImGui::CollapsingHeader("Camera Work", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::SeparatorText("Debug Camera");
		if (ImGui::Checkbox("Use Debug Camera", &isDebugCameraActive_))
		{
			if (isDebugCameraActive_ && !prevDebugCamera)
			{
				auto* activeCam = sceneManager_->GetCameraManager()->GetActiveCamera();
				debugCamera_->Start(activeCam->GetTranslate(), activeCam->GetRotate());
			}
			else if (!isDebugCameraActive_ && prevDebugCamera)
			{
				debugCamera_->Stop();
			}
		}
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