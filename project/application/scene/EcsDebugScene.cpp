#include "EcsDebugScene.h"
#include "../../engine/ecs/components/TransformComponent.h"
#include "../ecs/components/EnemyStateComponent.h"
#include "../../engine/ecs/components/LifetimeComponent.h"
#include "../../engine/ecs/components/HierarchyComponent.h"
#include "../../engine/ecs/components/InstancedRenderComponent.h"

#include "../ecs/systems/EnemyBehaviorSystem.h"
#include "../../engine/ecs/system/HierarchySystem.h"
#include "../../engine/ecs/system/LifetimeSystem.h"
#include "../../engine/ecs/system/InstancedRenderSystem.h"

#include "engine/manager/graphics/ModelManager.h"
#include "engine/manager/system/SrvManager.h"
#include "engine/graphics/3d/Object3dCommon.h"
#include "engine/scene/manager/SceneManager.h"

#include "time/TimeManager.h"
#include "externals/imgui/imgui.h"

EcsDebugScene::EcsDebugScene()
{
}

EcsDebugScene::~EcsDebugScene()
{
}

void EcsDebugScene::Initialize()
{
    // 1. Registry の構築
    constexpr uint32_t kMaxEntities = 50000;
    registry_ = std::make_unique<Registry>();
    registry_->Initialize(kMaxEntities);

    // 2. コンポーネント登録
    registry_->RegisterComponent<TransformComponent>(kMaxEntities);
    registry_->RegisterComponent<HierarchyComponent>(10000);
    registry_->RegisterComponent<LifetimeComponent>(kMaxEntities);
    registry_->RegisterComponent<EnemyStateComponent>(kMaxEntities);
    registry_->RegisterComponent<InstancedRenderComponent>(kMaxEntities);

    // 3. モデルロード
    ModelManager::GetInstance()->LoadModel("cube");

    // 4. レンダラとカメラの初期化
    if (sceneManager_)
    {
        DirectXCommon* dxCommon = sceneManager_->GetObject3dCommon()->GetDXCommon();
        SrvManager* srvManager = sceneManager_->GetObject3dCommon()->GetSrvManager();
        Model* model = ModelManager::GetInstance()->FindModel("cube");
        
        instancedRenderer_ = std::make_unique<InstancedModelRenderer>(kMaxEntities);
        instancedRenderer_->Initialize(dxCommon, srvManager, model);

        debugCamera_ = std::make_unique<DebugCamera>();
        debugCamera_->Initialize(sceneManager_->GetCameraManager()->GetActiveCamera());
        debugCamera_->Start({ 0.0f, 10.0f, -30.0f }, { 0.3f, 0.0f, 0.0f });
    }

    inspector_ = std::make_unique<EcsInspector>();
    inspector_->Initialize();

    systemManager_ = std::make_unique<SystemManager>();
    systemManager_->AddSystem(std::make_unique<EnemyBehaviorSystem>());
    systemManager_->AddSystem(std::make_unique<HierarchySystem>());
    systemManager_->AddSystem(std::make_unique<LifetimeSystem>());
    systemManager_->AddSystem(std::make_unique<InstancedRenderSystem>());

    // 動作開始
    StartState(SceneState::Playing);
}

void EcsDebugScene::Finalize()
{
}

void EcsDebugScene::CommonUpdate()
{
    float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;

    // --- デバッグUI ---
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
            EntityID entity = registry_->CreateEntity();
            if (entity == kInvalidEntity)
            {
                continue;
            }

            TransformComponent transform;
            float rx = ((rand() % 1000) / 10.0f) - 50.0f;
            float ry = ((rand() % 1000) / 10.0f);
            float rz = ((rand() % 1000) / 10.0f) - 50.0f;
            transform.localPosition_ = {rx, ry, rz};
            registry_->AddComponent<TransformComponent>(entity, transform);

            LifetimeComponent life;
            life.maxLifetime_ = 10.0f + (rand() % 5);
            life.currentAge_ = 0.0f;
            registry_->AddComponent<LifetimeComponent>(entity, life);

            InstancedRenderComponent rc;
            rc.modelName_ = "cube";
            rc.useInstancing_ = true;
            registry_->AddComponent<InstancedRenderComponent>(entity, rc);
        }
    }
    
    if (ImGui::Button("Spawn Dummy Enemies"))
    {
        for (uint32_t i = 0; i < spawnCountPerClick_; ++i)
        {
            EntityID entity = registry_->CreateEntity();
            if (entity != kInvalidEntity)
            {
                TransformComponent transform{};
                transform.localPosition_.x = ((rand() % 2000) / 10.0f) - 100.0f;
                transform.localPosition_.y = ((rand() % 500) / 10.0f);
                transform.localPosition_.z = ((rand() % 2000) / 10.0f) - 100.0f;
                registry_->AddComponent<TransformComponent>(entity, transform);

                EnemyStateComponent state{};
                state.currentState_ = EnemyStateComponent::State::Idle;
                registry_->AddComponent<EnemyStateComponent>(entity, state);

                InstancedRenderComponent render{};
                render.modelName_ = "cube";
                render.useInstancing_ = true;
                registry_->AddComponent<InstancedRenderComponent>(entity, render);

                LifetimeComponent life{};
                life.maxLifetime_ = 5.0f + (static_cast<float>(rand()) / RAND_MAX) * 5.0f;
                registry_->AddComponent<LifetimeComponent>(entity, life);
            }
        }
    }

    ImGui::End();

    // ECS状態表示
    inspector_->Draw(*registry_);

    // カメラ更新
    if (debugCamera_)
    {
        debugCamera_->Update();
    }

    // --- メインループ ---
    systemManager_->Update(*registry_);

    // 破棄実行
    registry_->FlushGarbageCollection();
}

void EcsDebugScene::Draw3D()
{
    BaseScene::Draw3D();
    
    if (sceneManager_ && instancedRenderer_)
    {
        Camera* camera = sceneManager_->GetCameraManager()->GetActiveCamera();
        LightManager* lightManager = sceneManager_->GetLightManager();
        ShadowMapManager* shadowMapManager = sceneManager_->GetShadowMapManager();

        auto irs = systemManager_->GetSystem<InstancedRenderSystem>();
        if (irs)
        {
            irs->Draw(*registry_, *instancedRenderer_, camera, lightManager, shadowMapManager);
        }
    }
}

void EcsDebugScene::Draw2D()
{
}

