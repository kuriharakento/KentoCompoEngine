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
	// ゲームBGMをループ再生で開始
	Audio::GetInstance()->LoadWave("game_bgm", "bgm/game.wav", SoundGroup::BGM);
	Audio::GetInstance()->PlayWave("game_bgm", true);
	Audio::GetInstance()->SetVolume("game_bgm", 0.2f);

	// イントロ演出用の初期カメラ位置を設定
	sceneManager_->GetCameraManager()->GetActiveCamera()->SetTranslate(cameraInitialPosition_);
	sceneManager_->GetCameraManager()->GetActiveCamera()->SetRotate(cameraInitialRotation_);

	// スカイドームの生成（背景天球）
	skydome_ = std::make_unique<Object3d>();
	skydome_->Initialize(sceneManager_->GetObject3dCommon());
	skydome_->SetModel("skydome");
	skydome_->SetLightManager(sceneManager_->GetLightManager());
	skydome_->SetEnableLighting(true);
	skydome_->SetDirectionalLightIntensity(0.5f);
	skydome_->SetDirectionalLightDirection({ 0.0f, -1.0f, 0.0f });

	// 地面の生成（UVスケールで地形テクスチャをタイル状に繰り返し）
	ground_ = std::make_unique<Object3d>();
	ground_->Initialize(sceneManager_->GetObject3dCommon());
	ground_->SetModel("terrain");
	ground_->SetLightManager(sceneManager_->GetLightManager());
	ground_->SetEnableLighting(true);
	ground_->GetModel()->SetUVScale(Vector3(10.0f, 10.0f, 1.0f));

	// 当たり判定システムの初期化
	CollisionManager::GetInstance()->Initialize();

	// コンボシステムの初期化とリセット
	ComboManager::GetInstance().Initialize(sceneManager_->GetSpriteCommon());
	ComboManager::GetInstance().Reset();

	// ステージデータのロード（敵配置、エリア設定等）
	stageManager_ = std::make_unique<StageManager>();
	stageManager_->Initialize(
		sceneManager_->GetObject3dCommon(),
		sceneManager_->GetLightManager(),
		sceneManager_->GetCameraManager()
	);
	stageManager_->LoadStage("stage_2");

	// ミニマップの初期化（ステージ情報を渡す）
	minimap_ = std::make_unique<Minimap>();
	minimap_->Initialize(sceneManager_->GetSpriteCommon(), stageManager_.get());

	// スプラインカメラの生成（演出用の滑らかなカメラパス）
	splineCamera_ = std::make_unique<SplineCamera>();
	splineCamera_->Initialize(sceneManager_->GetCameraManager()->GetActiveCamera());
	splineCamera_->LoadJson("spline.json");
	splineCamera_->Start(0.001f, false);
	splineCamera_->SetTarget(&stageManager_->GetPlayer()->GetPosition());

	// トップダウンカメラの生成（ゲームプレイ用の俯瞰視点）
	topDownCamera_ = std::make_unique<TopDownCamera>();
	topDownCamera_->Initialize(sceneManager_->GetCameraManager()->GetActiveCamera());
	topDownCamera_->SetOffset({ -45.0f, 0.0f, -28.0f });
	topDownCamera_->SetPitch(0.7f);
	topDownCamera_->SetYaw(1.0f);
	topDownCamera_->SetHeight(43.0f);

	// オービットカメラの生成（特定オブジェクトを中心に回転するカメラ）
	orbitCamera_ = std::make_unique<OrbitCameraWork>();
	orbitCamera_->Initialize(sceneManager_->GetCameraManager()->GetActiveCamera());

	// カーネージモードの初期化（コンボ達成時の強化システム）
	carnageMode_ = std::make_unique<CarnageMode>(stageManager_->GetPlayer());

	// シーン遷移エフェクトの設定（右下から左上へのフェードアウト）
	transitionEffect_.Initialize(
		sceneManager_->GetSpriteCommon(),
		"./Resources/black.png",
		22, 16,
		WinApp::kClientWidth, WinApp::kClientHeight
	);
	transitionEffect_.SetEaseType(SceneTransitionEase::InSine);
	transitionEffect_.SetFadeType(FadeType::FadeOut);
	transitionEffect_.SetMode(TransitionMode::RightBottomToLeftTop);

	// プレイヤー死亡時の画面エフェクト初期化
	playerDeathEffect_.Initialize(
		stageManager_->GetPlayer()
	);

	// 映画的演出用のレターボックスエフェクト初期化
	cinematicLetterbox_.Initialize(
		sceneManager_->GetSpriteCommon(),
		"./Resources/black.png",
		WinApp::kClientWidth, WinApp::kClientHeight
	);

	// シーンのライフサイクルをEnter状態から開始
	StartState(SceneState::Enter);
	gameClear_ = false;
	gameOver_ = false;
}

// --------- 終了処理 ---------
void GamePlayScene::Finalize()
{
	// BGMの停止
	Audio::GetInstance()->StopWave("game_bgm");

	// 当たり判定システムのクリーンアップ
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
	// 黒から赤へのフェードイン演出を開始
	transitionEffect_.Start(
		1.5f,
		VectorColorCodes::Black,
		VectorColorCodes::Red
	);
}

void GamePlayScene::OnUpdateEnter()
{
	transitionEffect_.Update();

	// フェード演出が完了したらイントロ状態へ遷移
	if (transitionEffect_.GetState() == TransitionState::Done)
	{
		ChangeState(SceneState::Intro);
	}
}

void GamePlayScene::OnExitEnter()
{
	// Enter状態の退場処理（現状は特になし）
}

// ==================================================
// Intro状態（ゲーム開始前のイントロ演出）
// ==================================================
void GamePlayScene::OnEnterIntro()
{
	// イントロ演出用タイマーのリセット
	introElapsed_ = 0.0f;
}

void GamePlayScene::OnUpdateIntro()
{
	// デルタタイムの取得
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

	// 初期位置から目標位置への補間
	Vector3 nextPos = MathUtils::Lerp(cameraInitialPosition_, targetPos, eased);
	Vector3 nextRot = {
		LerpAngle(cameraInitialRotation_.x, targetRot.x, eased),
		LerpAngle(cameraInitialRotation_.y, targetRot.y, eased),
		LerpAngle(cameraInitialRotation_.z, targetRot.z, eased)
	};

	activeCamera->SetTranslate(nextPos + topDownCamera_->GetOffset());
	activeCamera->SetRotate(nextRot);

	// イントロ演出完了判定
	if (t >= 1.0f)
	{
		// 最終位置を確実に設定してPlaying状態へ遷移
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
	// トップダウンカメラをプレイヤー追従で開始
	topDownCamera_->Start(
		43.0f,
		&stageManager_->GetPlayer()->GetPosition()
	);
}

void GamePlayScene::OnUpdatePlaying()
{
	// ゲーム終了条件の判定（早期リターンでパフォーマンス向上）

	// ステージクリア判定
	if (stageManager_->IsStageCleared())
	{
		ChangeState(SceneState::End);
		gameClear_ = true;
		return;
	}

	// ゲームオーバー判定（プレイヤー死亡）
	if (!stageManager_->GetPlayer()->IsAlive())
	{
		ChangeState(SceneState::End);
		gameOver_ = true;
		return;
	}

	// 当たり判定用の前フレーム位置を保存
	CollisionManager::GetInstance()->UpdatePreviousPositions();

	// カメラをプレイヤーに追従
	topDownCamera_->Update();

	// ミニマップの表示更新
	minimap_->Update();

	// ステージ内の全オブジェクト更新（プレイヤー、敵、エリア等）
	stageManager_->Update();

	// 背景要素の更新
	skydome_->Update(sceneManager_->GetCameraManager());
	ground_->Update(sceneManager_->GetCameraManager());

	// 全オブジェクト間の当たり判定を実行
	CollisionManager::GetInstance()->CheckCollisions();

	// コンボシステムの更新（タイマー管理）
	ComboManager::GetInstance().Update();

	// カーネージモードの更新（タイマー、エフェクト、発動条件チェック）
	carnageMode_->Update();
}

void GamePlayScene::OnExitPlaying()
{
	// Playing状態の退場処理（現状は特になし）
}

// ==================================================
// End状態（ゲーム終了演出：クリア/ゲームオーバー）
// ==================================================
void GamePlayScene::OnEnterEnd()
{
	// ゲームオーバー演出初期化
	if (gameOver_)
	{
		// 色収差エフェクト有効化
		auto* ppm = sceneManager_->GetPostProcessManager();
		ppm->crtEffect_->SetEnabled(true);
		ppm->crtEffect_->SetCrtEnabled(true);
		ppm->crtEffect_->SetChromaticAberrationEnabled(true);

		// プレイヤー死亡エフェクト開始
		playerDeathEffect_.Play(1.5f);
	}
	// ゲームクリア演出初期化
	else if (gameClear_)
	{
		// レターボックス開始
		cinematicLetterbox_.Show(1.0f);

		// タイマーを作成
		auto timer = std::make_unique<Timer>("GameClearToExitTimer", 1.5f, DeltaTimeType::RealDeltaTime);
		timer->SetDuration(1.5f);

		timer->SetOnFinish([this]() {
			ChangeState(SceneState::Exit);
						   });

		TimerManager::GetInstance().AddTimer(std::move(timer));

		// カメラをオービットモードに切り替え
		orbitCamera_->Start(
			&stageManager_->GetPlayer()->GetPosition(),
			15.0f,   // 半径
			0.5f     // 速度
		);
	}
}

void GamePlayScene::OnUpdateEnd()
{
	if (gameOver_)
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
	else if (gameClear_)
	{
		cinematicLetterbox_.Update();
		orbitCamera_->Update();
	}
}

void GamePlayScene::OnExitEnd()
{
	// End状態の退場処理（現状は特になし）
}

// ==================================================
// Exit状態（シーン退場・次シーンへの遷移）
// ==================================================
void GamePlayScene::OnEnterExit()
{
	// エッジから中心へのフェードイン演出を開始
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

	// フェード演出完了で次のシーンへ遷移
	if (transitionEffect_.GetState() == TransitionState::Done)
	{
		// ゲーム終了状態に応じて適切なシーンへ遷移
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
	// 色収差エフェクトを無効化してクリーンな状態で次シーンへ
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
	// 状態に関わらず常に更新が必要な要素
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
	skydome_->Draw();
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

	// レターボックスエフェクトの描画
	cinematicLetterbox_.Draw();

	// シーン遷移エフェクトの描画
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