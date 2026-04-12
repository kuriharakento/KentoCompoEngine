#include "TitleScene.h"

// engine/ecs
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/InstancedRenderComponent.h"
#include "engine/ecs/system/InstancedRenderSystem.h"
#include "engine/ecs/system/HierarchySystem.h"

// engine/graphics
#include "engine/manager/graphics/ModelManager.h"
#include "engine/graphics/3d/InstancedModelRenderer.h"
#include "engine/graphics/3d/Object3dCommon.h"

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
	ParticleManager::GetInstance()->LoadEffectDefinition("title_direction", "./Resources/json/particle/title_direction.json"); // 追加
	auto particleEffect = ParticleManager::GetInstance()->Play("title_particle", Vector3());

	// ディレクショナルライトの調整（下向き）
	DirectionalLight dirLight = sceneManager_->GetLightManager()->GetDirectionalLight();
	dirLight.direction = kLightDirection;
	dirLight.intensity = kLightIntensity;
	sceneManager_->GetLightManager()->SetDirectionalLight(dirLight);

	sceneManager_->GetCameraManager()->GetActiveCamera()->SetTranslate(Vector3(0.0f, kCameraHeight, kCameraInitialZ));
	sceneManager_->GetCameraManager()->GetActiveCamera()->SetRotate(Vector3());

	// --- ECS の初期化 ---
	registry_ = std::make_unique<Registry>();
	registry_->Initialize(1000); // タイトル用なので1000で十分
	registry_->RegisterComponent<TransformComponent>(1000);
	registry_->RegisterComponent<InstancedRenderComponent>(1000);

	Object3dCommon* objCommon = sceneManager_->GetObject3dCommon();
	ModelManager* modelManager = ModelManager::GetInstance();

	// レンダラーの準備
	auto setupRenderer = [&](const std::string& name, uint32_t count) {
		Model* model = modelManager->FindModel(name);
		if (model) {
			auto renderer = std::make_unique<InstancedModelRenderer>(count);
			renderer->Initialize(objCommon->GetDXCommon(), objCommon->GetSrvManager(), model);
			instancedRenderers_[name] = std::move(renderer);
		}
	};
	setupRenderer("weak_enemy", 500);
	setupRenderer("chicken", 1);

	// --- Entity 生成 ---
	// プレイヤー
	playerEntity_ = registry_->CreateEntity();
	registry_->AddComponent<TransformComponent>(playerEntity_, { {0, 1.0f, 0}, {0,0,0}, {1,1,1} }); // 高さを1に
	registry_->AddComponent<InstancedRenderComponent>(playerEntity_, { "chicken" });

	// 敵 300体
	const int kEnemyCount = 300;
	srand(static_cast<unsigned int>(time(NULL)));
	for (int i = 0; i < kEnemyCount; ++i)
	{
		EntityID enemy = registry_->CreateEntity();
		
		// プレイヤーの周り (半径12〜35m) にランダム配置
		float angle = (360.0f / kEnemyCount) * i * (3.14159f / 180.0f);
		float radius = 12.0f + static_cast<float>(rand() % 230) * 0.1f;
		float x = cosf(angle) * radius;
		float z = sinf(angle) * radius;

		// プレイヤー（中心）の方を向かせる
		Vector3 pos = { x, 1.0f, z }; // 高さを1に
		Vector3 toPlayer = Vector3(0, 0, 0) - pos;
		float rotY = atan2f(toPlayer.x, toPlayer.z);

		registry_->AddComponent<TransformComponent>(enemy, { pos, {0, rotY, 0}, {1,1,1} });
		registry_->AddComponent<InstancedRenderComponent>(enemy, { "weak_enemy" });
	}

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
	skydome_->SetScale({ 10.0f, 10.0f, 10.0f }); // 上昇しても端が見えないよう10倍に拡大
	skydome_->SetCastShadow(false);
	RegisterObject(skydome_.get());

	cube_.center = Vector3(0.0f, -100.0f, 0.0f); // 画面外へ
	cube_.size = Vector3(1.0f, 1.0f, 1.0f);

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

	// フォントスプライトの初期化
	fontSprite_ = std::make_unique<FontSprite>();
	fontSprite_->Initialize(sceneManager_->GetSpriteCommon(), "luna");
	fontSprite_->SetText("Press Click To Start");
	fontSprite_->SetPosition({ kFontPositionX, kFontPositionY });
	fontSprite_->SetScale(kFontScale);
	fontSprite_->SetColor(VectorColorCodes::Cyan);

	// カメラ初期設定
	cameraDistance_ = 35.0f; // さらに引く
	cameraOrbitAngle_ = 0.0f;

	// 行列計算システムの初期化と初回更新
	hierarchySystem_ = std::make_shared<HierarchySystem>();
	hierarchySystem_->Update(*registry_);

	StartState(SceneState::Playing);
}

void TitleScene::Finalize()
{
	ClearObjects();
	sceneManager_->GetPostProcessManager()->crtEffect_->SetEnabled(false);

	Audio::GetInstance()->StopWave("title_bgm");
}

void TitleScene::OnEnterPlaying()
{
}

void TitleScene::OnUpdatePlaying()
{
	DrawImGui();
	float dt = 0.0166f; // 固定

	fontSprite_->Update();

	if (!isTriggered_)
	{
		// 行列更新
		hierarchySystem_->Update(*registry_);

		// --- クリックで開始 ---
		bool isTrigger = false;
#ifdef _DEBUG
		// デバッグ時は右クリックのみ
		isTrigger = Input::GetInstance()->IsMouseButtonTriggered(2);
#else
		// 通常時は左右どちらかのクリック
		isTrigger = Input::GetInstance()->IsMouseButtonTriggered(0) || Input::GetInstance()->IsMouseButtonTriggered(2);
#endif

		if (isTrigger)
		{
			isTriggered_ = true;
			Audio::GetInstance()->PlayWave("start_se", false);
			
			// 背景エフェクトを即座に再生
			ParticleManager::GetInstance()->Play("title_direction", { 0, 1.0f, 0 });
			ParticleManager::GetInstance()->Play("explosion", { 0, 1.0f, 0 });

			// トランジション開始
			transitionEffect_.SetEaseType(SceneTransitionEase::InSine);
			transitionEffect_.SetFadeType(FadeType::FadeIn);
			transitionEffect_.SetMode(TransitionMode::LeftTopToRightBottom);
			transitionEffect_.Start(kTransitionDuration, VectorColorCodes::Red, VectorColorCodes::Black);

			// --- 敵を一斉に非表示 (ECS) ---
			auto array = registry_->View<InstancedRenderComponent>();
			int size = static_cast<int>(array->GetSize());
			for (int i = size - 1; i >= 0; --i)
			{
				EntityID entity = array->GetEntityFromDenseIndex(static_cast<uint32_t>(i));
				if (entity != playerEntity_)
				{
					registry_->RemoveComponent<InstancedRenderComponent>(entity);
				}
			}
		}

		// --- カメラの周回（オービット） ---
		cameraOrbitAngle_ += 0.005f;
		Vector3 cameraPos = {
			cosf(cameraOrbitAngle_) * cameraDistance_,
			3.0f, // 少し高い位置から
			sinf(cameraOrbitAngle_) * cameraDistance_
		};
		auto camera = sceneManager_->GetCameraManager()->GetActiveCamera();
		camera->SetTranslate(cameraPos);
		
		// 常に中央（プレイヤー）を向く
		Vector3 toTarget = cameraTarget_ - cameraPos;
		camera->SetRotate({ atan2f(-toTarget.y, sqrtf(toTarget.x * toTarget.x + toTarget.z * toTarget.z)), atan2f(toTarget.x, toTarget.z), 0.0f });
	}
	else
	{
		// --- 爆発演出シークエンス (シンプル・ズームアウト: 調整版) ---
		explosionTimer_ += dt;
		auto camera = sceneManager_->GetCameraManager()->GetActiveCamera();

		const float kZoomOutDuration = 1.2f; // 少しゆっくりに
		const float kInitialDist = 35.0f;
		const float kMaxDist = 65.0f;      // 引きすぎない距離
		const float kInitialFov = 0.45f;
		const float kMaxFov = 0.55f;       // FOVキックを控えめに
		
		float t = explosionTimer_ / kZoomOutDuration;
		t = (std::min)(1.0f, t);
		
		// 滑らかなズームアウト (EaseOutCubic)
		float easeT = 1.0f - powf(1.0f - t, 3.0f);

		// 距離と視野角の補間
		float currentDist = kInitialDist + (kMaxDist - kInitialDist) * easeT;
		float currentFov = kInitialFov + (kMaxFov - kInitialFov) * easeT;
		camera->SetFovY(currentFov);

		// 螺旋速度の増加を緩やかに
		cameraOrbitAngle_ += 0.02f * (1.0f + easeT * 1.5f);

		Vector3 cameraPos = {
			cosf(cameraOrbitAngle_) * currentDist,
			3.0f + 7.0f * easeT, // 高さも抑えめに
			sinf(cameraOrbitAngle_) * currentDist
		};
		camera->SetTranslate(cameraPos);

		// 常に中央を向く
		Vector3 toTarget = cameraTarget_ - cameraPos;
		camera->SetRotate({ atan2f(-toTarget.y, sqrtf(toTarget.x * toTarget.x + toTarget.z * toTarget.z)), atan2f(toTarget.x, toTarget.z), 0.0f });

		// 激しいカメラシェイク
		if (explosionTimer_ < 0.3f)
		{
			Vector3 offset = { (rand() % 100 - 50) * 0.03f, (rand() % 100 - 50) * 0.03f, (rand() % 100 - 50) * 0.03f };
			camera->SetTranslate(camera->GetTranslate() + offset);
		}

		if (transitionEffect_.GetState() == TransitionState::Done)
		{
			ChangeState(SceneState::Exit);
		}
	}

	transitionEffect_.Update();
	titleLogo_->Update();
	skydome_->Update(sceneManager_->GetCameraManager());
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

void TitleScene::Draw3D()
{
	LineManager::GetInstance()->DrawGrid(
		kGridSize,
		kGridSpacing,
		VectorColorCodes::DarkGray
	);

	// --- ECS インスタンス描画 ---
	InstancedRenderSystem::DrawGrouped(
		*registry_,
		instancedRenderers_,
		sceneManager_->GetCameraManager()->GetActiveCamera(),
		sceneManager_->GetLightManager(),
		sceneManager_->GetShadowMapManager()
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