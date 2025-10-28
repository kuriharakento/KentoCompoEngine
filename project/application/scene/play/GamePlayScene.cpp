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
#include "effects/particle/component/group/MaterialColorComponent.h"
#include "effects/particle/component/group/UVTranslateComponent.h"
#include "effects/particle/component/single/AccelerationComponent.h"
#include "effects/particle/component/single/BounceComponent.h"
#include "effects/particle/component/single/ColorFadeOutComponent.h"
#include "effects/particle/component/single/DragComponent.h"
#include "effects/particle/component/single/GravityComponent.h"
#include "effects/particle/component/single/OrbitComponent.h"
#include "effects/particle/component/single/RandomInitialVelocityComponent.h"
#include "effects/particle/component/single/RotationComponent.h"
#include "effects/particle/component/single/ScaleOverLifetimeComponent.h"
#include "time/TimeManager.h"
#include "time/TimerManager.h"

void GamePlayScene::Initialize()
{
	// 音声のロード・再生
	Audio::GetInstance()->LoadWave("game_bgm", "bgm/game.wav", SoundGroup::BGM);
	Audio::GetInstance()->PlayWave("game_bgm", true);
	Audio::GetInstance()->SetVolume("game_bgm", 0.2f);

	// カメラ設定
	sceneManager_->GetCameraManager()->GetActiveCamera()->SetTranslate(cameraInitialPosition_);
	sceneManager_->GetCameraManager()->GetActiveCamera()->SetRotate(cameraInitialRotation_);

	// スカイドームの生成
	skydome_ = std::make_unique<Object3d>();
	skydome_->Initialize(sceneManager_->GetObject3dCommon());
	skydome_->SetModel("skydome");
	skydome_->SetLightManager(sceneManager_->GetLightManager());
	skydome_->SetEnableLighting(true);
	skydome_->SetDirectionalLightIntensity(0.5f);
	skydome_->SetDirectionalLightDirection({ 0.0f, -1.0f, 0.0f });

	// 地面の生成
	ground_ = std::make_unique<Object3d>();
	ground_->Initialize(sceneManager_->GetObject3dCommon());
	ground_->SetModel("terrain");
	ground_->SetLightManager(sceneManager_->GetLightManager());
	ground_->SetEnableLighting(true);
	ground_->GetModel()->SetUVScale(Vector3(10.0f, 10.0f, 1.0f));

	// 当たり判定マネージャーの初期化
	CollisionManager::GetInstance()->Initialize();

	// Comboマネージャーの初期化
	ComboManager::GetInstance().Initialize(sceneManager_->GetSpriteCommon());
	ComboManager::GetInstance().Reset();

	// ステージマネージャーの生成
	stageManager_ = std::make_unique<StageManager>();
	stageManager_->Initialize(
		sceneManager_->GetObject3dCommon(),
		sceneManager_->GetLightManager(),
		sceneManager_->GetCameraManager()
	);
	stageManager_->LoadStage("field1");

	minimap_ = std::make_unique<Minimap>();
	minimap_->Initialize(sceneManager_->GetSpriteCommon(), stageManager_.get());

	// スプラインカメラの生成
	splineCamera_ = std::make_unique<SplineCamera>();
	splineCamera_->Initialize(sceneManager_->GetCameraManager()->GetActiveCamera());
	splineCamera_->LoadJson("spline.json");
	splineCamera_->Start(0.001f, false);
	splineCamera_->SetTarget(&stageManager_->GetPlayer()->GetPosition());

	// トップダウンカメラの生成
	topDownCamera_ = std::make_unique<TopDownCamera>();
	topDownCamera_->Initialize(sceneManager_->GetCameraManager()->GetActiveCamera());
	topDownCamera_->SetOffset({ 0.0f, 0.0f, -50.0f });
	topDownCamera_->SetPitch(0.9f);
	topDownCamera_->SetHeight(60.0f);

	// カーネージモードの初期化
	carnageMode_ = std::make_unique<CarnageMode>(stageManager_->GetPlayer());

	// シーン遷移エフェクトの初期化（フェードの開始は Enter フックで行う）
	transitionEffect_.Initialize(
		sceneManager_->GetSpriteCommon(),
		"./Resources/black.png",
		22, 16,
		1280.0f, 720.0f
	);
	transitionEffect_.SetEaseType(SceneTransitionEase::InSine);
	transitionEffect_.SetFadeType(FadeType::FadeOut);
	transitionEffect_.SetMode(TransitionMode::RightBottomToLeftTop);

	// 初期状態はEnter
	StartState(SceneState::Enter);
	gameClear_ = false;
	gameOver_ = false;
}

// --------- 終了処理 ---------
void GamePlayScene::Finalize()
{
	// bgm停止
	Audio::GetInstance()->StopWave("game_bgm");

	CollisionManager::GetInstance()->Finalize();
}

// ----------------------------------------------------------------
// 状態フック実装
// ----------------------------------------------------------------

// Enter（シーン開始 / フェードイン）
void GamePlayScene::OnEnterEnter()
{
	// 開始時のフェード
	transitionEffect_.Start(
		1.5f,
		VectorColorCodes::Black,
		VectorColorCodes::Red
	);
}

void GamePlayScene::OnUpdateEnter()
{
	// フェード更新
	transitionEffect_.Update();

	// フェード完了で Playing に遷移
	if (transitionEffect_.GetState() == TransitionState::Done)
	{
		ChangeState(SceneState::Intro);
	}
}

void GamePlayScene::OnExitEnter()
{
	// 特になし（必要ならクリーンアップ）
}

void GamePlayScene::OnEnterIntro()
{
	// タイマーのリセット
	introElapsed_ = 0.0f;
}

void GamePlayScene::OnUpdateIntro()
{
	// dt を取得（TimeManager の API を使用）
	float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;

	introElapsed_ += deltaTime;
	float t = introElapsed_ / introDuration_;
	if (t > 1.0f) t = 1.0f;
	float eased = EaseInOutCirc(t);

	auto activeCamera = sceneManager_->GetCameraManager()->GetActiveCamera();

	// --- 目標を取得する ---
	Vector3 targetPos = stageManager_->GetPlayer()->GetPosition() + Vector3{ 0.0f, topDownCamera_->GetHeight(), 0.0f };
	Vector3 targetRot = { topDownCamera_->GetPitch(), topDownCamera_->GetYaw(), 0.0f };

	// --- 補間 ---
	Vector3 nextPos = MathUtils::Lerp(cameraInitialPosition_, targetPos, eased);
	Vector3 nextRot = {
		LerpAngle(cameraInitialRotation_.x, targetRot.x, eased),
		LerpAngle(cameraInitialRotation_.y, targetRot.y, eased),
		LerpAngle(cameraInitialRotation_.z, targetRot.z, eased)
	};

	activeCamera->SetTranslate(nextPos + topDownCamera_->GetOffset());
	activeCamera->SetRotate(nextRot);

	// --- 完了判定 ---
	if (t >= 1.0f)
	{
		// 最終合わせ
		activeCamera->SetTranslate(targetPos + topDownCamera_->GetOffset());
		activeCamera->SetRotate(targetRot);
		ChangeState(SceneState::Playing);
	}
}

// Playing（メインの更新ロジックをここに移動）
void GamePlayScene::OnEnterPlaying()
{
	// トップダウンカメラを開始
	topDownCamera_->Start(
		60.0f,
		&stageManager_->GetPlayer()->GetPosition()
	);
}

void GamePlayScene::OnUpdatePlaying()
{
	// ゲーム終了判定
	// クリア
	if (stageManager_->IsStageCleared())
	{
		ChangeState(SceneState::End);
		gameClear_ = true;
		return;
	}

	// ゲームオーバー
	if (!stageManager_->GetPlayer()->IsAlive())
	{
		ChangeState(SceneState::End);
		gameOver_ = true;
		return;
	}

	// プレイ会用
	if(Input::GetInstance()->TriggerKey(DIK_SPACE))
	{
		// ゲームオーバー演出へ
		ChangeState(SceneState::End);
		gameOver_ = true;
	}


	// 遷移エフェクト更新（安定化のため常に Update）
	transitionEffect_.Update();

	// 前フレームの位置を更新
	CollisionManager::GetInstance()->UpdatePreviousPositions();

	// カメラの更新
	topDownCamera_->Update();

	// ミニマップの更新
	minimap_->Update();

	// ステージの更新
	stageManager_->Update();

	// スカイドームの更新
	skydome_->Update(sceneManager_->GetCameraManager());

	// 地面の更新
	ground_->Update(sceneManager_->GetCameraManager());

	// 衝突判定開始
	CollisionManager::GetInstance()->CheckCollisions();

	// コンボマネージャーの更新
	ComboManager::GetInstance().Update();

	// カーネージモードの更新
	carnageMode_->Update();
}

void GamePlayScene::OnExitPlaying()
{
}

// End（終了演出）
void GamePlayScene::OnEnterEnd()
{
	// 色収差エフェクト有効化
	auto* ppm = sceneManager_->GetPostProcessManager();
	ppm->crtEffect_->SetEnabled(true);
	ppm->crtEffect_->SetCrtEnabled(true);
	ppm->crtEffect_->SetChromaticAberrationEnabled(true);

	// プレイヤー死亡エフェクト初期化
	playerDeathEffect_.Initialize(
		stageManager_->GetPlayer()
	);
	playerDeathEffect_.Play(1.5f);
}

void GamePlayScene::OnUpdateEnd()
{
	// プレイヤー死亡エフェクト更新
	playerDeathEffect_.Update();

	// 時間経過
	gameOverEffectElapsed_ += TimeManager::GetInstance().GetGameContext().deltaTime;

	// 色収差を振幅で揺らす
	const float frequencyHz = 4.0f;  // 1秒間に4回揺れる
	const float maxOscAmp = 35.0f;   // 初期最大振幅
	const float decayRate = 2.8f;    // 大きいほど速く収束

	// 指数減衰で自然に収束させる
	float envelope = maxOscAmp * std::exp(-decayRate * gameOverEffectElapsed_);
	if (envelope < 0.001f) 
	{
		// 微小値切り捨て
		envelope = 0.0f;
	}

	const float twoPi = std::numbers::pi_v<float> *2.0f;
	float oscill = std::sinf(gameOverEffectElapsed_ * frequencyHz * twoPi) * envelope;

	// 純粋な振幅をセット（ベース値は加えない）
	auto* ppm = sceneManager_->GetPostProcessManager();
	ppm->crtEffect_->SetChromaticAberrationOffset(oscill);

	if (playerDeathEffect_.IsFinished())
	{
		ChangeState(SceneState::Exit);
	}
}

void GamePlayScene::OnExitEnd()
{
}

void GamePlayScene::OnEnterExit()
{
	// 終了演出の開始
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
	// 色収差エフェクト無効化
	auto* ppm = sceneManager_->GetPostProcessManager();
	ppm->crtEffect_->SetEnabled(false);
	ppm->crtEffect_->SetCrtEnabled(false);
	ppm->crtEffect_->SetChromaticAberrationEnabled(false);
}

void GamePlayScene::CommonUpdate()
{
	skydome_->Update(sceneManager_->GetCameraManager());
	ground_->Update(sceneManager_->GetCameraManager());
	stageManager_->UpdateTransforms(sceneManager_->GetCameraManager());
}

// ----------------------------------------------------------------
// 描画
// ----------------------------------------------------------------

void GamePlayScene::Draw3D()
{
	// スカイドームの描画
	skydome_->Draw();

	// 地面の描画
	ground_->Draw();

	// ステージの描画
	stageManager_->Draw();

	// スプライン曲線の描画
	splineCamera_->DrawSplineLine();
}

void GamePlayScene::Draw2D()
{
	// ミニマップの描画
	minimap_->Draw();

	// コンボUIの描画
	ComboManager::GetInstance().Draw();

	// シーン遷移エフェクトの描画
	transitionEffect_.Draw();
}

void GamePlayScene::DrawImGui()
{
#ifdef _DEBUG
	ImGui::Begin("GameScene");

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