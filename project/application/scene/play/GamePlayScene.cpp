#include "GamePlayScene.h"

// audio
#include "audio/Audio.h"
// scene
#include "engine/scene/manager/SceneManager.h"
#include "application/ui/LevelUpUI.h"
// editor
#include "externals/imgui/imgui.h"
// input
#include "input/Input.h"
// math
#include "math/MatrixFunc.h"
#include "math/VectorColorCodes.h"
// graphics
#include "manager/graphics/LineManager.h"
#include "manager/effect/PostProcessManager.h"
#include "engine/graphics/3d/Object3dCommon.h"
#include "engine/manager/scene/LightManager.h"
#include "engine/manager/scene/CameraManager.h"
#include "engine/graphics/3d/InstancedModelRenderer.h"
#include "engine/manager/graphics/ShadowMapManager.h"
#include "graphics/2d/SpriteCommon.h"

// app
#include "engine/gameobject/component/collision/CollisionManager.h"
// components
#include "application/combo/ComboManager.h"
#include "application/GameObject/component/action/PistolComponent.h"
#include "time/TimeManager.h"
#include "application/effect/BulletTrailManager.h"
#include "effects/particle/ParticleManager.h"
#include "effects/particle/module/spawn/SpawnShapeModules.h" // SpawnShapeModule
#include "application/UI/PoseMenu.h"
#include "base/WinApp.h"

// ECS Integration
#include "engine/ecs/system/HierarchySystem.h"
#include "application/ecs/systems/EnemyBehaviorSystem.h"
#include "engine/ecs/system/InstancedRenderSystem.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/HierarchyComponent.h"
#include "engine/ecs/components/TagComponent.h" // ecs::TagComponent
#include "engine/ecs/components/MovementComponent.h"
#include "engine/ecs/components/ColliderComponent.h"
#include "application/ecs/components/PlayerProgressionComponent.h"
#include "application/ecs/components/SkillComponent.h"
#include "application/ecs/components/DodgeComponent.h"
#include "application/ecs/components/ProjectileComponent.h"
#include "application/ecs/components/ImpactChargeComponent.h"
#include "application/ecs/components/EnemyStateComponent.h"
#include "application/ecs/components/StatusComponent.h"
#include "engine/ecs/components/EnemyAIComponent.h"
#include "engine/ecs/components/InstancedRenderComponent.h"
#include "engine/ecs/components/CollisionResponseComponent.h"
#include "application/ecs/components/InducedExplosionComponent.h"
#include "engine/ecs/components/LifetimeComponent.h"
#include "application/ecs/components/ObstacleComponent.h"
#include "application/ecs/components/BulletComponent.h"
#include "application/ecs/components/PlayerComponent.h"
#include "application/ecs/components/EnemyTypeComponent.h"
#include "application/ecs/components/EnemyChargerComponent.h"
#include "application/ecs/CollisionConfig.h"
#include "application/ecs/components/PlayerProgressionComponent.h"
#include "application/ecs/components/SkillComponent.h"
#include "application/ecs/components/StatusComponent.h"
#include "engine/ecs/components/TagComponent.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/ColliderComponent.h"
#include "engine/ecs/components/WorldBoundaryComponent.h"

// Systems
#include "engine/ecs/system/HierarchySystem.h"
#include "engine/ecs/system/MovementSystem.h"
#include "engine/ecs/system/CollisionSystem.h"
#include "engine/ecs/system/LifetimeSystem.h"
#include "engine/ecs/system/PlayerSystem.h"
#include "application/ecs/systems/PlayerActionSystem.h"
#include "application/ecs/systems/ProgressionSystem.h"
#include "application/ecs/systems/ProjectileSystem.h"
#include "application/ecs/systems/AnnihilationSystem.h"
#include "engine/ecs/system/EcsStatusSystem.h"
#include "engine/ecs/system/WorldBoundarySystem.h"

void GamePlayScene::Initialize()
{
	// --- エンジン・基盤の初期化 ---
	DirectionalLight mainLight;
	mainLight.direction = kLightDirection;
	mainLight.intensity = kLightIntensity;
	mainLight.color = { 1.0f, 1.0f, 1.0f, 1.0f }; // デフォルト色
	sceneManager_->GetLightManager()->SetDirectionalLight(mainLight);
	sceneManager_->GetObject3dCommon()->SetDefaultLightManager(sceneManager_->GetLightManager());

	skydome_ = std::make_unique<Object3d>();
	skydome_->Initialize(sceneManager_->GetObject3dCommon());
	skydome_->SetModel("skydome");
	skydome_->SetScale({ kSkydomeScale, kSkydomeScale, kSkydomeScale });
	skydome_->SetLightManager(sceneManager_->GetLightManager());

	ground_ = std::make_unique<Object3d>();
	ground_->Initialize(sceneManager_->GetObject3dCommon());
	ground_->SetModel("terrain");
	ground_->SetLightManager(sceneManager_->GetLightManager());

	// --- ECS の初期化 ---
	registry_ = std::make_unique<Registry>();
	registry_->Initialize(10000);

	// コンポーネント登録
	registry_->RegisterComponent<TransformComponent>(10000);
	registry_->RegisterComponent<HierarchyComponent>(10000);
	registry_->RegisterComponent<ecs::TagComponent>(10000);
	registry_->RegisterComponent<MovementComponent>(10000);
	registry_->RegisterComponent<InstancedRenderComponent>(10000);
	registry_->RegisterComponent<ecs::ColliderComponent>(10000);
	registry_->RegisterComponent<PlayerProgressionComponent>(1);
	registry_->RegisterComponent<SkillComponent>(1);
	registry_->RegisterComponent<DodgeComponent>(1);
	registry_->RegisterComponent<ProjectileComponent>(10000);
	registry_->RegisterComponent<ImpactChargeComponent>(10000);
	registry_->RegisterComponent<ecs::StatusComponent>(10000);
	registry_->RegisterComponent<CollisionResponseComponent>(10000);
	registry_->RegisterComponent<PlayerComponent>(1);
	registry_->RegisterComponent<EnemyAIComponent>(5000);
	registry_->RegisterComponent<EnemyStateComponent>(5000);
	registry_->RegisterComponent<LifetimeComponent>(10000);
	registry_->RegisterComponent<ecs::InducedExplosionComponent>(5000);
	registry_->RegisterComponent<ObstacleComponent>(1000);
	registry_->RegisterComponent<BulletComponent>(10000);
	registry_->RegisterComponent<EnemyTypeComponent>(10000);   // 敵種別タグ
	registry_->RegisterComponent<EnemyChargerComponent>(5000); // 突進型コンポーネント
	registry_->RegisterComponent<WorldBoundaryComponent>(1);  // フィールド全体で1つ

	systemManager_ = std::make_unique<SystemManager>();

	// 1. 生成・スポーン系
	enemySpawnSystem_ = std::make_shared<EnemySpawnSystem>();
	enemySpawnSystem_->Initialize(
		sceneManager_->GetObject3dCommon(),
		sceneManager_->GetLightManager(),
		sceneManager_->GetCameraManager()
	);
	systemManager_->AddSystem(enemySpawnSystem_);

	// 2. 移動・アクション系 (localPosition の更新)
	auto playerSystem = std::make_shared<PlayerSystem>();
	playerSystem->SetCameraManager(sceneManager_->GetCameraManager());
	systemManager_->AddSystem(playerSystem);

	auto playerActionSystem = std::make_shared<PlayerActionSystem>();
	playerActionSystem->SetCameraManager(sceneManager_->GetCameraManager());
	playerActionSystem->SetSystemManager(systemManager_.get());
	systemManager_->AddSystem(playerActionSystem);

	// パーティクル定義のロード
	ParticleManager::GetInstance()->LoadEffectDefinition("enemy_death", "./Resources/json/particle/enemy_death.json");
	ParticleManager::GetInstance()->LoadEffectDefinition("E_skill", "./Resources/json/particle/E_skill.json");
	ParticleManager::GetInstance()->LoadEffectDefinition("E_explosion", "./Resources/json/particle/E_explosion.json");
	ParticleManager::GetInstance()->LoadEffectDefinition("hit_effect_ver2", "./Resources/json/particle/hit_effect_ver2.json");
	ParticleManager::GetInstance()->LoadEffectDefinition("move_range", "./Resources/json/particle/move_range.json");
	ParticleManager::GetInstance()->LoadEffectDefinition("R_skill", "./Resources/json/particle/R_skill.json");
	ParticleManager::GetInstance()->LoadEffectDefinition("Q_skill", "./Resources/json/particle/Q_skill.json");

	// 移動制限範囲の可視化エフェクトを開始
	rangeEffect_ = ParticleManager::GetInstance()->Play("move_range", { 0.0f, 0.1f, 0.0f });
	if (rangeEffect_)
	{
		rangeEffect_->SetAutoRemove(false);
	}

	systemManager_->AddSystem(std::make_shared<EnemyBehaviorSystem>());
	systemManager_->AddSystem(std::make_shared<MovementSystem>());
	systemManager_->AddSystem(std::make_shared<ProjectileSystem>());
	auto progressionSystem = std::make_shared<ProgressionSystem>();
	systemManager_->AddSystem(progressionSystem);
	systemManager_->AddSystem(std::make_shared<WorldBoundarySystem>());

	// 3. 行列更新・物理計算 (worldMatrix の構築)
	// 移動後に実行することで、最新の座標をワールド行列に反映させる
	systemManager_->AddSystem(std::make_shared<HierarchySystem>());

	// 4. 衝突判定・解決
	// 行列更新後に実行することで、正しい座標で判定を行う
	systemManager_->AddSystem(std::make_shared<CollisionSystem>());

	// 5. 状態更新・ライフサイクル
	systemManager_->AddSystem(std::make_shared<AnnihilationSystem>());
	systemManager_->AddSystem(std::make_shared<LifetimeSystem>());
	systemManager_->AddSystem(std::make_shared<EcsStatusSystem>());

	// 6. 描画準備
	systemManager_->AddSystem(std::make_shared<InstancedRenderSystem>());

	// --- モデルとインスタンスレンダラーの準備 ---
	ModelManager::GetInstance()->LoadModel("enemy");
	ModelManager::GetInstance()->LoadModel("weak_enemy", ".gltf");
	ModelManager::GetInstance()->LoadModel("tank_enemy", ".gltf");
	ModelManager::GetInstance()->LoadModel("player");

	Object3dCommon* obj3dCommon = sceneManager_->GetObject3dCommon();

	// 敵用レンダラー (共通・互換用)
	Model* enemyModel = ModelManager::GetInstance()->FindModel("enemy");
	if (enemyModel)
	{
		auto renderer = std::make_unique<InstancedModelRenderer>(5000); // 最大5000体
		renderer->Initialize(
			obj3dCommon->GetDXCommon(),
			obj3dCommon->GetSrvManager(),
			enemyModel
		);
		instancedRenderers_["enemy"] = std::move(renderer);
	}

	// 近接型敵用レンダラー
	Model* weakEnemyModel = ModelManager::GetInstance()->FindModel("weak_enemy");
	if (weakEnemyModel)
	{
		auto renderer = std::make_unique<InstancedModelRenderer>(5000);
		renderer->Initialize(
			obj3dCommon->GetDXCommon(),
			obj3dCommon->GetSrvManager(),
			weakEnemyModel
		);
		instancedRenderers_["weak_enemy"] = std::move(renderer);
	}

	// 突進型敵用レンダラー
	Model* tankEnemyModel = ModelManager::GetInstance()->FindModel("tank_enemy");
	if (tankEnemyModel)
	{
		auto renderer = std::make_unique<InstancedModelRenderer>(1000); // 突進型は少なめ
		renderer->Initialize(
			obj3dCommon->GetDXCommon(),
			obj3dCommon->GetSrvManager(),
			tankEnemyModel
		);
		instancedRenderers_["tank_enemy"] = std::move(renderer);
	}

	// プレイヤー用レンダラー
	Model* playerModel = ModelManager::GetInstance()->FindModel("chicken");
	if (playerModel)
	{
		auto renderer = std::make_unique<InstancedModelRenderer>(1); // 1体
		renderer->Initialize(
			obj3dCommon->GetDXCommon(),
			obj3dCommon->GetSrvManager(),
			playerModel
		);
		instancedRenderers_["chicken"] = std::move(renderer);
	}

	// --- プレイヤーEntityの生成 ---
	playerEntity_ = registry_->CreateEntity();
	if (playerEntity_ != kInvalidEntity)
	{
		ecs::TagComponent playerTag;
		playerTag.type = ecs::TagComponent::Type::Player;
		registry_->AddComponent<ecs::TagComponent>(playerEntity_, playerTag);

		registry_->AddComponent<TransformComponent>(playerEntity_, { {0.0f, 1.0f, 0.0f}, {0,0,0}, { 1.5f,1.5f,1.5f } });
		registry_->AddComponent<PlayerProgressionComponent>(playerEntity_, {});

		SkillComponent skill;
		// 初期状態ではLMBのみアンロック（デフォルト値を使用）
		registry_->AddComponent<SkillComponent>(playerEntity_, skill);

		registry_->AddComponent<DodgeComponent>(playerEntity_, {});
		registry_->AddComponent<ecs::StatusComponent>(playerEntity_, ecs::StatusComponent{});

		// コライダー設定
		ecs::ColliderComponent col;
		col.type_ = ColliderType::Sphere;
		col.sphere_.radius = 1.0f;

		// フィルタリング設定
		col.layer = CollisionLayer::Player;
		col.mask = CollisionLayer::Enemy | CollisionLayer::Obstacle;

		registry_->AddComponent<ecs::ColliderComponent>(playerEntity_, col);

		registry_->AddComponent<CollisionResponseComponent>(playerEntity_, {});

		// プレイヤーコンポーネント追加
		registry_->AddComponent<PlayerComponent>(playerEntity_, {});

		// 描画コンポーネント追加
		InstancedRenderComponent render;
		render.modelName_ = "chicken";
		registry_->AddComponent<InstancedRenderComponent>(playerEntity_, render);
		// 移動制限を追加 (100.0f)
		// 容量は1なので、プレイヤー以外には付与できない（メモリ最小限）
		registry_->AddComponent<WorldBoundaryComponent>(playerEntity_, { 100.0f, true });
	}

	// --- 敵管理の初期化 ---
	enemyManager_ = std::make_unique<EnemyManager>();

	// 引数の型を明示的に渡す (コンパイラの推論エラー対策)
	Registry* reg = registry_.get();
	SystemManager* sys = systemManager_.get();
	Object3dCommon* objCommon = sceneManager_->GetObject3dCommon();
	SpriteCommon* sprCommon = sceneManager_->GetSpriteCommon();
	CameraManager* cam = sceneManager_->GetCameraManager();
	LightManager* light = sceneManager_->GetLightManager();
	ShadowMapManager* shadow = sceneManager_->GetShadowMapManager();

	enemyManager_->Initialize(
		reg,
		sys,
		objCommon,
		sprCommon,
		cam,
		light,
		shadow,
		nullptr
	);

	// --- カメラの初期化 ---
	Camera* activeCam = sceneManager_->GetCameraManager()->GetActiveCamera();

	topDownCamera_ = std::make_unique<TopDownCamera>();
	topDownCamera_->Initialize(sceneManager_->GetCameraManager()->GetActiveCamera());
	topDownCamera_->SetActive(true); // [BNS-Fix] カメラの更新を有効化
	constexpr float kTopDownOffsetX = -45.0f;
	constexpr float kTopDownOffsetZ = -28.0f;
	topDownCamera_->SetOffset({ kTopDownOffsetX, 0.0f, kTopDownOffsetZ });
	topDownCamera_->SetPitch(kTopDownCameraPitch);
	topDownCamera_->SetYaw(kTopDownCameraYaw);
	topDownCamera_->SetHeight(kTopDownCameraHeight);

	orbitCamera_ = std::make_unique<OrbitCameraWork>();
	orbitCamera_->Initialize(activeCam);

	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize(activeCam);

	splineCamera_ = std::make_unique<SplineCamera>();
	splineCamera_->Initialize(activeCam);

	// --- UI・演出の初期化 ---
	reticle_ = std::make_unique<Cursor>();
	reticle_->Initialize(sceneManager_->GetSpriteCommon(), "./Resources/UI/reticle.png");

	controlsGuide_ = std::make_unique<ControlsGuide>();
	controlsGuide_->Initialize(sceneManager_->GetSpriteCommon(), registry_.get(), playerEntity_);

	controlsGuide_->SetVisible(true);

	levelUpUI_ = std::make_unique<LevelUpUI>();
	levelUpUI_->Initialize(sceneManager_->GetSpriteCommon());

	// ProgressionSystem に通知先を設定
	progressionSystem->SetLevelUpUI(levelUpUI_.get());
	progressionSystem->SetPostProcessManager(sceneManager_->GetPostProcessManager());

	// シーン遷移エフェクトの初期化
	const std::string transitionPath = "./Resources/black.png";
	const float sW = static_cast<float>(WinApp::kClientWidth);
	const float sH = static_cast<float>(WinApp::kClientHeight);

	transitionEffect_.Initialize(
		sprCommon,
		transitionPath,
		static_cast<int>(kTransitionGridX),
		static_cast<int>(kTransitionGridY),
		sW,
		sH
	);

	// シネマティックレターボックスの初期化
	cinematicLetterbox_.Initialize(
		sprCommon,
		transitionPath,
		sW,
		sH
	);

	// 初期ステートをセットして起動
	StartState(SceneState::Enter);
}

void GamePlayScene::Finalize()
{
	BulletTrailManager::GetInstance().Clear();
	if (registry_) registry_.reset();
}

void GamePlayScene::Draw3D()
{
	skydome_->Draw();
	ground_->Draw();

	// --- インスタンス描画 (一括呼び出し) ---
	InstancedRenderSystem::DrawGrouped(
		*registry_,
		instancedRenderers_,
		sceneManager_->GetCameraManager()->GetActiveCamera(),
		sceneManager_->GetLightManager(),
		sceneManager_->GetShadowMapManager()
	);

	// --- ECS システムの描画 (衝突判定の可視化、スポーン範囲など) ---
	if (systemManager_)
	{
		systemManager_->Draw(
			*registry_,
			sceneManager_->GetCameraManager()->GetActiveCamera(),
			sceneManager_->GetLightManager(),
			sceneManager_->GetShadowMapManager()
		);
	}
}

void GamePlayScene::DrawShadow()
{
}

void GamePlayScene::Draw2D()
{
	DrawUI();
	transitionEffect_.Draw();
	cinematicLetterbox_.Draw();
}

void GamePlayScene::UpdateUI()
{
	if (reticle_) reticle_->Update();
	if (controlsGuide_) controlsGuide_->Update();
	if (levelUpUI_) levelUpUI_->Update();
}

void GamePlayScene::DrawUI()
{
	if (reticle_) reticle_->Draw();
	if (controlsGuide_) controlsGuide_->Draw();
	if (levelUpUI_) levelUpUI_->Draw();
}

void GamePlayScene::DrawImGui()
{
#ifdef USE_IMGUI
	// --- ECS Central Hub ---
	ImGui::Begin("ECS Debug Hub");

	if (registry_)
	{
		ImGui::Text("Active Entities: %d", registry_->GetActiveEntityCount());
		ImGui::Separator();
	}

	// 1. プレイヤーステータス
	if (registry_ && playerEntity_ != kInvalidEntity)
	{
		if (ImGui::CollapsingHeader("Player Stats", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// HP (StatusComponent)
			if (registry_->HasComponent<ecs::StatusComponent>(playerEntity_))
			{
				auto& status = registry_->GetComponent<ecs::StatusComponent>(playerEntity_);
				float hp = status.hp_.GetValue();
				float maxHp = status.maxHp_.GetValue();
				ImGui::Text("HP: %.1f / %.1f", hp, maxHp);
				ImGui::ProgressBar(hp / maxHp, ImVec2(-1.0f, 0.0f));
			}

			// レベル・経験値 (PlayerProgressionComponent)
			if (registry_->HasComponent<PlayerProgressionComponent>(playerEntity_))
			{
				auto& prog = registry_->GetComponent<PlayerProgressionComponent>(playerEntity_);
				ImGui::Text("Level: %d", prog.level_);
				ImGui::Text("Exp: %.1f / %.1f", prog.currentExp_, prog.nextLevelExp_);
				ImGui::ProgressBar(prog.currentExp_ / prog.nextLevelExp_, ImVec2(-1.0f, 0.0f));
			}
		}

		// 2. スキル情報
		if (registry_->HasComponent<SkillComponent>(playerEntity_))
		{
			if (ImGui::CollapsingHeader("Skills", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto& skill = registry_->GetComponent<SkillComponent>(playerEntity_);

				auto drawSkillInfo = [](const char* name, bool unlocked, float timer) {
					ImGui::Text("%-8s: %s (Timer: %.2f)",
								name,
								unlocked ? "Unlocked" : "Locked",
								timer > 0 ? timer : 0.0f);
					};

				drawSkillInfo("LMB", skill.isLmbUnlocked_, skill.lmbTimer_);
				drawSkillInfo("RMB", skill.isRmbUnlocked_, skill.rmbTimer_);
				drawSkillInfo("Q", skill.isDecoyUnlocked_, skill.decoyTimer_);
				drawSkillInfo("E", skill.isImpactUnlocked_, skill.impactTimer_);
				drawSkillInfo("R", skill.isBeamUnlocked_, skill.beamTimer_);
			}
		}
	}

	// 3. フィールド制限設定
	if (registry_ && playerEntity_ != kInvalidEntity)
	{
		if (registry_->HasComponent<WorldBoundaryComponent>(playerEntity_))
		{
			if (ImGui::CollapsingHeader("World Boundary", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto& boundary = registry_->GetComponent<WorldBoundaryComponent>(playerEntity_);
				ImGui::SliderFloat("Boundary Radius", &boundary.radius_, 0.0f, 1000.0f);
				ImGui::Checkbox("Boundary Active", &boundary.active_);
			}
		}
	}

	// 4. スポーン設定
	if (enemySpawnSystem_)
	{
		if (ImGui::CollapsingHeader("Spawn Settings", ImGuiTreeNodeFlags_DefaultOpen))
		{
			float inner = enemySpawnSystem_->GetInnerRadius();
			float outer = enemySpawnSystem_->GetOuterRadius();

			if (ImGui::SliderFloat("Inner Radius", &inner, 0.0f, 50.0f))
			{
				enemySpawnSystem_->SetInnerRadius(inner);
			}
			if (ImGui::SliderFloat("Outer Radius", &outer, inner, 100.0f))
			{
				enemySpawnSystem_->SetOuterRadius(outer);
			}
		}
	}

	ImGui::End();
#endif
}

void GamePlayScene::OnEnterEnter()
{
	transitionEffect_.SetFadeType(FadeType::FadeOut);
	transitionEffect_.Start(kEnterTransitionDuration, VectorColorCodes::Red, VectorColorCodes::Black);
}

void GamePlayScene::OnUpdateEnter()
{
	if (transitionEffect_.GetState() == TransitionState::Done)
	{
		ChangeState(SceneState::Playing);
	}
}

void GamePlayScene::OnExitEnter()
{
}

void GamePlayScene::OnEnterIntro()
{
	introElapsed_ = 0.0f;
}

void GamePlayScene::OnUpdateIntro()
{
	if (Input::GetInstance()->TriggerKey(DIK_P))
	{
		ChangeState(SceneState::Playing);
	}
}

void GamePlayScene::OnEnterPlaying()
{
	// プレイヤーの位置をカメラのターゲットに設定
	topDownCamera_->SetTarget(&registry_->GetComponent<TransformComponent>(playerEntity_).localPosition_);
}

void GamePlayScene::OnUpdatePlaying()
{
	if (!registry_ || !systemManager_) return;

	// すべてのECSシステムを更新
	systemManager_->Update(*registry_);

	// 従来の管理マネージャーの更新
	enemyManager_->Update();

	// 予約されたエンティティを物理削除
	registry_->FlushGarbageCollection();

	// 簡易的になゲームオーバー判定 (プレイヤーHPが0以下)
	if (playerEntity_ != kInvalidEntity && registry_->HasComponent<ecs::StatusComponent>(playerEntity_))
	{
		auto& status = registry_->GetComponent<ecs::StatusComponent>(playerEntity_);
		if (status.hp_.GetValue() <= 0.0f)
		{
			gameOver_ = true;
			ChangeState(SceneState::End);
		}
	}

	// ゲームクリア判定 (時間の経過)
	gameTime_ += TimeManager::GetInstance().GetGameContext().deltaTime;
	if (gameTime_ >= 180.0f)
	{
		gameClear_ = true;
		ChangeState(SceneState::End);
	}
}

void GamePlayScene::OnExitPlaying()
{
}

void GamePlayScene::OnEnterEnd()
{
	transitionEffect_.SetFadeType(FadeType::FadeIn);
	transitionEffect_.Start(kEnterTransitionDuration, VectorColorCodes::Red, VectorColorCodes::Black);
}

void GamePlayScene::OnUpdateEnd()
{
	if (transitionEffect_.GetState() == TransitionState::Done)
	{
		if(gameOver_)
		{
			// ゲームオーバー画面へ
			sceneManager_->ChangeScene(SceneNames::GameOver);
		}
		else if (gameClear_)
		{
			// ゲームクリア画面へ
			sceneManager_->ChangeScene(SceneNames::GameClear);
		}
	}
}

void GamePlayScene::OnExitEnd()
{
}

void GamePlayScene::OnEnterExit()
{
	// FadeIn（黒が現れる ＝ シーン終了）
	transitionEffect_.SetFadeType(FadeType::FadeIn);
	transitionEffect_.Start(kExitTransitionDuration, { 0,0,0,1 }, { 0,0,0,1 });
}

void GamePlayScene::OnUpdateExit()
{
	if (transitionEffect_.GetState() == TransitionState::Done)
	{
		// 次のシーンへ
	}
}

void GamePlayScene::OnExitExit()
{
}

void GamePlayScene::CommonUpdate()
{
	transitionEffect_.Update();
	cinematicLetterbox_.Update();
	UpdateUI();

	// カメラの更新
	if (isDebugCameraActive_) debugCamera_->Update();
	else if (gameClear_) orbitCamera_->Update();
	else topDownCamera_->Update();

	// 背景オブジェクトの更新
	skydome_->Update();
	ground_->Update();

	// 移動制限範囲エフェクトの同期
	if (rangeEffect_ && registry_ && playerEntity_ != kInvalidEntity)
	{
		if (registry_->HasComponent<WorldBoundaryComponent>(playerEntity_))
		{
			auto& boundary = registry_->GetComponent<WorldBoundaryComponent>(playerEntity_);
			
			// 全エミッターの SpawnShapeModule を更新
			for (uint32_t i = 0; i < rangeEffect_->GetEmitterCount(); ++i)
			{
				auto* emitter = rangeEffect_->GetEmitter(i);
				if (auto* shape = emitter->GetModule<SpawnShapeModule>())
				{
					// innerRadius を境界半径に、outerRadius を +0.5 に設定
					shape->SetRadius(boundary.radius_, boundary.radius_ + 0.5f);
				}
			}

			// エフェクトの有効/無効を同期
			if (boundary.active_ && !rangeEffect_->IsPlaying()) rangeEffect_->Play();
			else if (!boundary.active_ && rangeEffect_->IsPlaying()) rangeEffect_->Stop();
		}
	}
}