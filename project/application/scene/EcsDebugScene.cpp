#include "EcsDebugScene.h"
#include "../ecs/components/TransformComponent.h"
#include "../ecs/components/EnemyStateComponent.h"
#include "../ecs/components/LifetimeComponent.h"
#include "../ecs/components/HierarchyComponent.h"
#include "../ecs/components/RenderComponent.h"

#include "../ecs/systems/EnemyBehaviorSystem.h"
#include "../ecs/systems/HierarchySystem.h"
#include "../ecs/systems/LifetimeSystem.h"
#include "../ecs/systems/InstancedRenderSystem.h"

#include "engine/manager/graphics/ModelManager.h"
#include "engine/manager/system/SrvManager.h"
#include "engine/graphics/3d/Object3dCommon.h"
#include "engine/scene/manager/SceneManager.h"

#include "time/TimeManager.h"
#include "externals/imgui/imgui.h"
#include <iostream>

EcsDebugScene::EcsDebugScene()
{
}

EcsDebugScene::~EcsDebugScene()
{
}

void EcsDebugScene::Initialize()
{
    // 1. Registry の初期化（最大50,000エンティティ）
    constexpr uint32_t kMaxEntities = 50000;
    registry_.Initialize(kMaxEntities);

    // 2. 各 ComponentArray のプールを事前確保
    registry_.RegisterComponent<TransformComponent>(kMaxEntities);
    registry_.RegisterComponent<HierarchyComponent>(10000);
    registry_.RegisterComponent<LifetimeComponent>(kMaxEntities);
    registry_.RegisterComponent<EnemyStateComponent>(kMaxEntities);
    registry_.RegisterComponent<RenderComponent>(kMaxEntities);

    // 3. モデルのロード
    ModelManager::GetInstance()->LoadModel("cube");
    Model* cubeModel = ModelManager::GetInstance()->FindModel("cube");

    // 4. レンダラとデバッガの初期化
    if (sceneManager_)
    {
        // インスタンシングレンダラーの初期化
        DirectXCommon* dxCommon = sceneManager_->GetObject3dCommon()->GetDXCommon();
        SrvManager* srvManager = sceneManager_->GetObject3dCommon()->GetSrvManager();
        Model* model = ModelManager::GetInstance()->FindModel("cube");
        
        instancedRenderer_ = std::make_unique<InstancedModelRenderer>(50000);
        instancedRenderer_->Initialize(dxCommon, srvManager, model);

        // デバッグカメラの初期化
        debugCamera_ = std::make_unique<DebugCamera>();
        debugCamera_->Initialize(sceneManager_->GetCameraManager()->GetActiveCamera());
        debugCamera_->Start({ 0.0f, 10.0f, -30.0f }, { 0.3f, 0.0f, 0.0f });
    }

    debugViewer_ = std::make_unique<EcsDebugViewer>();
    debugViewer_->Initialize();

    std::cout << "[EcsDebugScene] Initialized ECS Sandbox with capacity: " << kMaxEntities << "\n";
    
    // BaseSceneの初期化作法（StartState）
    StartState(SceneState::Playing);
}

void EcsDebugScene::Finalize()
{
}

void EcsDebugScene::CommonUpdate()
{
    float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;

    // --- ImGui によるデバッグ操作UI ---
    ImGui::Begin("ECS Sandbox Control");
    
    if (debugCamera_)
    {
        bool isActive = debugCamera_->IsActive();
        if (ImGui::Checkbox("Enable Debug Camera", &isActive))
        {
            if (isActive)
            {
                debugCamera_->Start();
            }
            else
            {
                debugCamera_->Stop();
            }
        }
        ImGui::Separator();
    }

    ImGui::SliderInt("Spawn Count", (int*)&spawnCountPerClick_, 1, 10000);

    if (ImGui::Button("Spawn 500 Cubes"))
    {
        for (uint32_t i = 0; i < 500; ++i)
        {
            EntityID entity = registry_.CreateEntity();
            if (entity == kInvalidEntity)
            {
                continue;
            }

            TransformComponent transform;
            // -50 ~ +50 の範囲でばらまく
            float rx = ((rand() % 1000) / 10.0f) - 50.0f;
            float ry = ((rand() % 1000) / 10.0f);
            float rz = ((rand() % 1000) / 10.0f) - 50.0f;
            transform.localPosition_ = {rx, ry, rz};
            registry_.AddComponent<TransformComponent>(entity, transform);

            // 適当な寿命
            LifetimeComponent life;
            life.maxLifetime_ = 10.0f + (rand() % 5);
            life.currentAge_ = 0.0f;
            registry_.AddComponent<LifetimeComponent>(entity, life);

            // モデル名は "cube" を指定
            RenderComponent rc;
            rc.modelName_ = "cube";
            rc.useInstancing_ = true;
            registry_.AddComponent<RenderComponent>(entity, rc);
        }
    }
    
    if (ImGui::Button("Spawn Dummy Enemies"))
    {
        for (uint32_t i = 0; i < spawnCountPerClick_; ++i)
        {
            EntityID entity = registry_.CreateEntity();
            if (entity != kInvalidEntity)
            {
                TransformComponent transform{};
                // 広範囲にランダム配置
                transform.localPosition_.x = ((rand() % 2000) / 10.0f) - 100.0f;
                transform.localPosition_.y = ((rand() % 500) / 10.0f);
                transform.localPosition_.z = ((rand() % 2000) / 10.0f) - 100.0f;
                registry_.AddComponent<TransformComponent>(entity, transform);

                EnemyStateComponent state{};
                registry_.AddComponent<EnemyStateComponent>(entity, state);

                // 「cube」モデルで描画
                RenderComponent render{};
                render.modelName_ = "cube";
                render.useInstancing_ = true;
                registry_.AddComponent<RenderComponent>(entity, render);

                // 5〜10秒のランダムな寿命を設定
                LifetimeComponent life{};
                life.maxLifetime_ = 5.0f + (static_cast<float>(rand()) / RAND_MAX) * 5.0f;
                registry_.AddComponent<LifetimeComponent>(entity, life);
            }
        }
    }

    ImGui::End();

    // ECS 状態のビューワー描画
    debugViewer_->DrawWindow(registry_);

    // デバッグカメラの更新
    if (debugCamera_)
    {
        debugCamera_->Update();
    }

    // --- メインループのパイプライン ---

    // 1. 各エンティティのAIやローカル位置の更新
    EnemyBehaviorSystem::Update(registry_, deltaTime);

    // 2. 親子関係の解決（LocalからWorldMatrixの計算）
    HierarchySystem::Update(registry_);

    // 3. 寿命判定（尽きたものはDeferredQueueへ）
    LifetimeSystem::Update(registry_, deltaTime);

    // 4. フレーム末尾の破棄実行
    registry_.FlushGarbageCollection();
}

void EcsDebugScene::Draw3D()
{
    // ECS以外を描画する場合
    BaseScene::Draw3D();
    
    if (sceneManager_ && instancedRenderer_)
    {
        Camera* camera = sceneManager_->GetCameraManager()->GetActiveCamera();
        LightManager* lightManager = sceneManager_->GetLightManager();
        ShadowMapManager* shadowMapManager = sceneManager_->GetShadowMapManager();

        // コンポーネント単位の描画
        InstancedRenderSystem::Draw(registry_, *instancedRenderer_, camera, lightManager, shadowMapManager);
    }
}

void EcsDebugScene::Draw2D()
{
}
