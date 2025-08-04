#include "TitleScene.h"

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

static Vector3 fallHeartPos = Vector3{ 2.0f, 2.0f, 0.0f };
static Vector3 glitchPos = Vector3{ -2.0f, 2.0f, 0.0f };
static Vector3 startPos = Vector3{ 0.0f, 2.0f, 0.0f };
static Vector3 mordeVFXPos = Vector3{ 0.0f, 0.0f, 0.0f };
static Vector3 origin = Vector3{ 0.0f, 3.0f, 0.0f };

void TitleScene::Initialize()
{
	Audio::GetInstance()->LoadWave("fanfare", "game.wav", SoundGroup::BGM);
	// 音声の再生
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

	// ステージマネージャーの生成
	stageManager_ = std::make_unique<StageManager>();
	stageManager_->Initialize(
		sceneManager_->GetObject3dCommon(),
		sceneManager_->GetLightManager()
	);
	stageManager_->LoadStage("field");

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

	// パーティクルエミッターの初期化
	InitializeParticleEmitters();

}

void TitleScene::Finalize()
{
	CollisionManager::GetInstance()->Finalize();
}

void TitleScene::Update()
{
	if(Input::GetInstance()->TriggerKey(DIK_TAB))
	{
		// ステージエディットシーン移動
		sceneManager_->ChangeScene("STAGEEDIT");
	}

	// ImGuiの描画
	DrawImGui();

	// 前フレームの位置を更新
	CollisionManager::GetInstance()->UpdatePreviousPositions();

	// カメラの更新
	topDownCamera_->Update();

	// ステージの更新
	stageManager_->Update();

	// スカイドームの更新
	skydome_->Update(sceneManager_->GetCameraManager());

	// 地面の更新
	ground_->Update(sceneManager_->GetCameraManager());

	// 衝突判定開始
	CollisionManager::GetInstance()->CheckCollisions();
}

void TitleScene::Draw3D()
{
	// スカイドームの描画
	skydome_->Draw();

	// 地面の描画
	ground_->Draw();

	// ステージの描画
	stageManager_->Draw(sceneManager_->GetCameraManager());

	// スプライン曲線の描画
	splineCamera_->DrawSplineLine();
}

void TitleScene::Draw2D()
{

}

void TitleScene::InitializeParticleEmitters()
{
#pragma region dust effect
	// エミッターの初期化（前回の設定をベースに調整）
	dust_ = std::make_unique<ParticleEmitter>();
	dust_->Initialize("dust", "./Resources/star.png");
	dust_->SetEmitRange({ -5.0f, -5.0f, -5.0f }, { 5.0f, 5.0f, 5.0f }); // 広めに設定
	dust_->Start(
		&origin, // 発生位置
		30,   // 30個のパーティクルを一度に生成（バースト）
		0.1f, // 0.1秒かけて全パーティクルを放出
		true // ループさせない（一回きりのバースト）
	);
	dust_->SetEmitRate(2.0f); // 定期的な放出はなし
	dust_->SetInitialLifeTime(0.3f); // 長めの寿命
	dust_->SetModelType(ParticleGroup::ParticleType::Plane); // 光の粒感
	dust_->SetBillborad(true); // カメラ常に正面を向く

	//======コンポーネントの追加=========================
	// 空気抵抗コンポーネントを追加 (徐々に減速し、漂う感じ)
	dust_->AddComponent(std::make_shared<DragComponent>(0.99f));
	dust_->AddComponent(std::make_shared<ScaleOverLifetimeComponent>(10.0f, 0.0f));
	// 色フェードアウトコンポーネント (寿命後半で透明になる)
	dust_->AddComponent(std::make_shared<ColorFadeOutComponent>());
	// 回転コンポーネント (ゆっくり回転)
	dust_->AddComponent(std::make_shared<RotationComponent>(Vector3{ 0.0f, 0.16f, 0.0f }));
	// マテリアル色変更コンポーネント (青系の光)
	dust_->AddComponent(std::make_shared<MaterialColorComponent>(VectorColorCodes::Gold));
#pragma endregion

#pragma region red effect
	// エミッターの初期化
	redEffect_ = std::make_unique<ParticleEmitter>();
	redEffect_->Initialize("redEffect", "./Resources/gradationLine.png"); // 縦長の光のテクスチャ
	redEffect_->SetEmitRange({ -0.1f, 0.0f, -0.1f }, { 0.1f, 0.0f, 0.1f }); // 地面付近で発生
	redEffect_->Start(
		&startPos,
		3,    // 1個ずつ連続的に生成
		1.0f, // 10秒間放出
		true  // ループさせる（継続的に光が上昇）
	);
	redEffect_->SetEmitRate(0.2f); // 0.5秒ごとに1個生成
	redEffect_->SetInitialLifeTime(2.0f); // 長めの寿命
	redEffect_->SetModelType(ParticleGroup::ParticleType::Ring);
	redEffect_->SetBillborad(false);

	//======コンポーネントの追加=========================
	// 加速度コンポーネント (上方向への加速)
	redEffect_->AddComponent(std::make_shared<AccelerationComponent>(Vector3{ 0.0f, 0.001f, 0.0f }));
	// スケール変化コンポーネント (小さく始まり、最大になり、消滅)
	redEffect_->AddComponent(std::make_shared<ScaleOverLifetimeComponent>(0.2f, 1.0f)); // 出現時は小さく、徐々に大きく。
	// 色フェードアウトコンポーネント (寿命後半でフェードアウト)
	redEffect_->AddComponent(std::make_shared<ColorFadeOutComponent>());
	// 回転コンポーネント (ゆっくりY軸回転)
	redEffect_->AddComponent(std::make_shared<RotationComponent>(Vector3{ 0.01f, 0.01f, 0.0f }));
	//UV変換コンポーネント (テクスチャの動き)
	redEffect_->AddComponent(std::make_shared<UVTranslateComponent>(Vector3{ 0.1f, 0.0f, 0.0f })); // UVを毎フレーム大きくずらす
	// マテリアル色変更コンポーネント (神秘的な光)
	redEffect_->AddComponent(std::make_shared<MaterialColorComponent>(VectorColorCodes::Red));
	redEffect_->SetRandomVelocity(true);
	redEffect_->SetRandomVelocityRange(AABB{ Vector3{ -0.3f,-0.3f,-0.3f }, Vector3{ 0.3f,0.3f,0.3f } });
	redEffect_->SetRandomRotation(true);
	redEffect_->SetRandomRotationRange(AABB{ Vector3{ -3.14f, 3.14f, 0.0f }, Vector3{ 3.14f, 3.14f, 0.0f } });
#pragma endregion

#pragma region glitch effect
	// エミッターの初期化
	glitch_ = std::make_unique<ParticleEmitter>();
	glitch_->Initialize("glitch", "./Resources/circle2.png"); // グリッチのテクスチャ
	glitch_->SetEmitRange({ -0.1f, -0.1f, -0.1f }, { 0.1f, 0.1f, 0.1f }); // 非常に狭く、オブジェクトの表面付近
	glitch_->Start(
		&glitchPos,
		50,   // 50個のパーティクルを一度に生成
		0.1f, // 非常に短い時間で放出（瞬時に大量発生）
		true // ループさせない
	);
	glitch_->SetEmitRate(4.0f); // 定期的な放出はなし
	glitch_->SetInitialLifeTime(4.f); // 非常に短い寿命
	glitch_->SetModelType(ParticleGroup::ParticleType::Plane); // デジタル感を出す
	glitch_->SetBillborad(true); // カメラ常に正面を向く

	//======コンポーネントの追加=========================
	// 初期速度ランダム化 (極端なランダム速度で瞬時に拡散)
	glitch_->AddComponent(std::make_shared<RandomInitialVelocityComponent>(
		Vector3{ -10.0f, -10.0f, -10.0f }, Vector3{ 10.0f, 10.0f, 10.0f }));
	// スケール変化コンポーネント (一瞬で現れて消える)
	glitch_->AddComponent(std::make_shared<ScaleOverLifetimeComponent>(0.0f, 1.0f));
	// 色フェードアウトコンポーネント (寿命後半で急激に透明になる)
	glitch_->AddComponent(std::make_shared<ColorFadeOutComponent>());

	// 回転コンポーネント (高速でランダムな回転)
	glitch_->AddComponent(std::make_shared<RotationComponent>(Vector3{ 0.5f, 0.5f, 0.5f }));
	// マテリアル色変更コンポーネント (蛍光色系)
	glitch_->AddComponent(std::make_shared<MaterialColorComponent>(VectorColorCodes::Magenta));
#pragma endregion

#pragma region fall heart effect
	// エミッターの初期化
	fallHeart_ = std::make_unique<ParticleEmitter>();
	fallHeart_->Initialize("fallHeart", "./Resources/star.png"); // ハートのテクスチャ
	fallHeart_->SetEmitRange({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }); // 発生ポイントを固定
	fallHeart_->Start(
		&fallHeartPos,
		1,
		0.0f,
		true // ループさせない
	);
	fallHeart_->SetEmitRate(0.1f); // 定期的な放出なし
	fallHeart_->SetInitialLifeTime(1.5f); // 短〜中程度の寿命
	fallHeart_->SetModelType(ParticleGroup::ParticleType::Heart); // ハート型！
	fallHeart_->SetBillborad(true); // カメラ常に正面を向く
	fallHeart_->SetRandomVelocity(true); // ランダムな初期速度を有効にする
	fallHeart_->SetRandomVelocityRange(AABB(Vector3{ -1.0f, 0.0f, -1.0f }, Vector3{ 1.0f, 3.0f, 1.0f }));

	//======コンポーネントの追加=========================
	// 重力コンポーネント (緩やかな重力で落下)
	fallHeart_->AddComponent(std::make_shared<GravityComponent>(Vector3{ 0.0f, -0.1f, 0.0f }));
	// スケール変化コンポーネント (出現してすぐ最大になり、徐々に小さく)
	fallHeart_->AddComponent(std::make_shared<ScaleOverLifetimeComponent>(0.5f, 0.0f));
	// 色フェードアウトコンポーネント (寿命後半でフェードアウト)
	fallHeart_->AddComponent(std::make_shared<ColorFadeOutComponent>());
	// 回転コンポーネント (可愛い回転)
	fallHeart_->AddComponent(std::make_shared<RotationComponent>(Vector3{ 0.0f, 0.0f, 0.1f }));
	// バウンスコンポーネント (地面で優しく跳ねる)
	fallHeart_->AddComponent(std::make_shared<BounceComponent>(0.0f, 0.3f, 0.0f));
	// マテリアル色変更コンポーネント (ピンク)
	fallHeart_->AddComponent(std::make_shared<MaterialColorComponent>(VectorColorCodes::Pink));
#pragma endregion

#pragma region mordeVFX
	mordeVFXGround_ = std::make_unique<ParticleEmitter>();
	mordeVFXGround_->Initialize("mordeVFXGround", "./Resources/gradationLine.png");
	mordeVFXGround_->SetEmitRange({ -0.0f, 0.0f, -0.0f }, { 0.0f, 0.0f, 0.0f });
	mordeVFXGround_->SetInitialLifeTime(0.02f);
	mordeVFXGround_->SetInitialScale(Vector3{ 3.0f,3.0f,3.0f });
	mordeVFXGround_->SetEmitRate(0.0f);
	//　地面に倒す角度にする
	mordeVFXGround_->SetInitialRotation(Vector3{ std::numbers::pi_v<float> *0.5, 0.0f, 0.0f });
	mordeVFXGround_->SetBillborad(false);
	mordeVFXGround_->Start(
		&mordeVFXPos,
		1,
		0.0f,
		true
	);
	mordeVFXGround_->SetModelType(ParticleGroup::ParticleType::Ring);
	//UV変換コンポーネント追加
	mordeVFXGround_->AddComponent(std::make_shared<UVTranslateComponent>(Vector3{ -0.2f, 0.0f, 0.0f })); // UVを毎フレーム大きくずらす
	// マテリアル色変更コンポーネント追加
	mordeVFXGround_->AddComponent(std::make_shared<MaterialColorComponent>(VectorColorCodes::Cyan));

	// 回ってる欠片を再現
	mordeVFXFragment_ = std::make_unique<ParticleEmitter>();
	mordeVFXFragment_->Initialize("mordeVFXFragment", "./Resources/star.png");
	mordeVFXFragment_->SetEmitRange({ -3.0f,0.0f,-3.0f }, { 3.0f,1.0f,3.0f });
	mordeVFXFragment_->SetInitialLifeTime(1.0f);
	mordeVFXFragment_->SetInitialScale(Vector3{ 0.7f,0.7f,0.7f });
	mordeVFXFragment_->SetRandomRotation(true);
	mordeVFXFragment_->SetRandomRotationRange(AABB{ Vector3{ -3.14f, 3.14f, 0.0f }, Vector3{ 3.14f, 3.14f, 0.0f } });
	mordeVFXFragment_->SetEmitRate(0.2f);
	mordeVFXFragment_->Start(
		&mordeVFXGround_->GetPosition(),
		6,
		1.0f,
		true
	);
	//軌道コンポーネントを追加
	mordeVFXFragment_->AddComponent(std::make_shared<OrbitComponent>(
		&mordeVFXFragment_->GetPosition(),
		2.0f, // 半径
		0.1f // 速度
	));
	//　回転コンポーネントを追加
	mordeVFXFragment_->AddComponent(std::make_shared<RotationComponent>(Vector3{ 0.1f, 0.1f, 0.0f }));
	// マテリアル色変更コンポーネント追加
	mordeVFXFragment_->AddComponent(std::make_shared<MaterialColorComponent>(VectorColorCodes::Cyan));
#pragma endregion
	redEffect_->StopEmit();
	dust_->StopEmit();
	fallHeart_->StopEmit();
	glitch_->StopEmit();
	mordeVFXGround_->StopEmit();
	mordeVFXFragment_->StopEmit();

}

void TitleScene::DrawImGui()
{
#ifdef _DEBUG
	ImGui::Begin("TitleScene");

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

	ImGui::SeparatorText("Particle Emitter");

	if (ImGui::CollapsingHeader("emitters"))
	{
		ImGui::DragFloat3("Start Position", &startPos.x, 0.01f);
		ImGui::DragFloat3("Glitch Position", &glitchPos.x, 0.01f);
		ImGui::DragFloat3("Fall Heart Position", &fallHeartPos.x, 0.01f);
		ImGui::DragFloat3("Morde VFX Position", &mordeVFXPos.x, 0.01f);
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
#pragma endregion

	ImGui::End();

#endif
}
