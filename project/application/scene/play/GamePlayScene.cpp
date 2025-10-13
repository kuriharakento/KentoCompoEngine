#include "GamePlayScene.h"

// audio
#include "audio/Audio.h"
// scene
#include "engine/scene/manager/SceneManager.h"
// editor
#include "externals/imgui/imgui.h"
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
    Audio::GetInstance()->LoadWave("fanfare", "game.wav", SoundGroup::BGM);
    Audio::GetInstance()->PlayWave("fanfare", true);

    sceneManager_->GetCameraManager()->GetActiveCamera()->SetTranslate(Vector3(0.0f, 1.5f, -15.0f));

    //スカイドームの生成
    skydome_ = std::make_unique<Object3d>();
    skydome_->Initialize(sceneManager_->GetObject3dCommon());
    skydome_->SetModel("skydome");
    skydome_->SetLightManager(sceneManager_->GetLightManager());
    skydome_->SetEnableLighting(true);
    skydome_->SetDirectionalLightIntensity(0.5f);
    //ディレクショナルライトを下から上に照らす
    skydome_->SetDirectionalLightDirection({ 0.0f, -1.0f, 0.0f });

    // 地面の生成
    ground_ = std::make_unique<Object3d>();
    ground_->Initialize(sceneManager_->GetObject3dCommon());
    ground_->SetModel("terrain");
    ground_->SetLightManager(sceneManager_->GetLightManager());
    ground_->SetEnableLighting(true);
    ground_->GetModel()->SetUVScale(Vector3(10.0f, 10.0f, 1.0f));

    //当たり判定マネージャーの初期化
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
    stageManager_->LoadStage("field");

    minimap_ = std::make_unique<Minimap>();
    minimap_->Initialize(sceneManager_->GetSpriteCommon(), stageManager_.get());

    //スプラインカメラの生成
    splineCamera_ = std::make_unique<SplineCamera>();
    splineCamera_->Initialize(sceneManager_->GetCameraManager()->GetActiveCamera());
    splineCamera_->LoadJson("spline.json");
    splineCamera_->Start(0.001f, false);
    splineCamera_->SetTarget(&stageManager_->GetPlayer()->GetPosition());

    //トップダウンカメラの生成
    topDownCamera_ = std::make_unique<TopDownCamera>();
    topDownCamera_->Initialize(sceneManager_->GetCameraManager()->GetActiveCamera());
    topDownCamera_->SetOffset({ 0.0f, 0.0f, -5.0f });
    topDownCamera_->SetPitch(0.9f);
    topDownCamera_->Start(
        60.0f,
        &stageManager_->GetPlayer()->GetPosition()
    );

    // デバッグカメラの生成
    debugCamera_ = std::make_unique<DebugCamera>();
    debugCamera_->Initialize(sceneManager_->GetCameraManager()->GetActiveCamera());
    debugCamera_->Start();

    // カーネージモードの初期化
    carnageMode_ = std::make_unique<CarnageMode>(stageManager_->GetPlayer());

	// シーン遷移エフェクトの初期化
	transitionEffect_.Initialize(
		sceneManager_->GetSpriteCommon(),
		"./Resources/black.png",
		18, 12,
		1280.0f, 720.0f
	);
	transitionEffect_.SetEaseType(SceneTransitionEase::InSine);
	transitionEffect_.SetFadeType(FadeType::FadeOut);
	transitionEffect_.SetMode(TransitionMode::RightBottomToLeftTop);
	transitionEffect_.Start(
		1.0f,
		VectorColorCodes::Red,
		VectorColorCodes::Black
	);
}

// --------- 終了処理 ---------
void GamePlayScene::Finalize()
{
    CollisionManager::GetInstance()->Finalize();
}

void GamePlayScene::Update()
{
    if (Input::GetInstance()->TriggerKey(DIK_TAB))
    {
        // ステージエディットシーン移動
        sceneManager_->ChangeScene("STAGEEDIT");
    }

    // ゲームクリア
    if (stageManager_->IsStageCleared())
    {
        sceneManager_->ChangeScene("GAMEPLAY");
    }

    // ImGuiの描画
    DrawImGui();

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
	if (useDebugCamera)
	{
		debugCamera_->Update();
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