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
    m_registry.Initialize(kMaxEntities);

    // 2. 各 ComponentArray のプールを事前確保
    m_registry.RegisterComponent<TransformComponent>(kMaxEntities);
    m_registry.RegisterComponent<HierarchyComponent>(10000);
    m_registry.RegisterComponent<LifetimeComponent>(kMaxEntities);
    m_registry.RegisterComponent<EnemyStateComponent>(kMaxEntities);
    m_registry.RegisterComponent<RenderComponent>(kMaxEntities);

    // 3. モデルのロード
    ModelManager::GetInstance()->LoadModel("cube");
    Model* cubeModel = ModelManager::GetInstance()->FindModel("cube");

    // 4. レンダラとデバッガの初期化
    if (sceneManager_)
    {
        // インスタンシングレンダラーの初期化 (500個)
        DirectXCommon* dxCommon = sceneManager_->GetObject3dCommon()->GetDXCommon();
        SrvManager* srvManager = sceneManager_->GetObject3dCommon()->GetSrvManager();
        Model* model = ModelManager::GetInstance()->FindModel("cube");
        
        m_instancedRenderer = std::make_unique<InstancedModelRenderer>(50000);
        m_instancedRenderer->Initialize(dxCommon, srvManager, model);

        // デバッグカメラの初期化
        m_debugCamera = std::make_unique<DebugCamera>();
        m_debugCamera->Initialize(sceneManager_->GetCameraManager()->GetActiveCamera());
        m_debugCamera->Start({ 0.0f, 10.0f, -30.0f }, { 0.3f, 0.0f, 0.0f });
    }

    m_debugViewer = std::make_unique<EcsDebugViewer>();
    m_debugViewer->Initialize();

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
    
    if (m_debugCamera)
    {
        bool isActive = m_debugCamera->IsActive();
        if (ImGui::Checkbox("Enable Debug Camera", &isActive))
        {
            if (isActive) m_debugCamera->Start();
            else m_debugCamera->Stop();
        }
        ImGui::Separator();
    }

    ImGui::SliderInt("Spawn Count", (int*)&m_spawnCountPerClick, 1, 10000);

    if (ImGui::Button("Spawn 500 Cubes"))
    {
        for (uint32_t i = 0; i < 500; ++i)
        {
            EntityID entity = m_registry.CreateEntity();
            if (entity == kInvalidEntity) continue;

            TransformComponent transform;
            // -50 ~ +50 の範囲でばらまく
            float rx = ((rand() % 1000) / 10.0f) - 50.0f;
            float ry = ((rand() % 1000) / 10.0f);
            float rz = ((rand() % 1000) / 10.0f) - 50.0f;
            transform.localPosition = {rx, ry, rz};
            m_registry.AddComponent<TransformComponent>(entity, transform);

            // 適当な寿命
            LifetimeComponent life;
            life.maxLifetime = 10.0f + (rand() % 5);
            life.currentAge = 0.0f;
            m_registry.AddComponent<LifetimeComponent>(entity, life);

            // モデル名は "cube" を指定 (ユーザー要望通り)
            RenderComponent rc;
            rc.modelName = "cube";
            rc.useInstancing = true;
            m_registry.AddComponent<RenderComponent>(entity, rc);
        }
    }
    
    if (ImGui::Button("Spawn Dummy Enemies"))
    {
        for (uint32_t i = 0; i < m_spawnCountPerClick; ++i)
        {
            EntityID entity = m_registry.CreateEntity();
            if (entity != kInvalidEntity)
            {
                TransformComponent transform{};
                // 広範囲にランダム配置
                transform.localPosition.x = ((rand() % 2000) / 10.0f) - 100.0f;
                transform.localPosition.y = ((rand() % 500) / 10.0f);
                transform.localPosition.z = ((rand() % 2000) / 10.0f) - 100.0f;
                m_registry.AddComponent<TransformComponent>(entity, transform);

                EnemyStateComponent state{};
                m_registry.AddComponent<EnemyStateComponent>(entity, state);

                // 「enemy」モデルで描画
                RenderComponent render{};
                render.modelName = "cube";
                render.useInstancing = true;
                m_registry.AddComponent<RenderComponent>(entity, render);

                // 5〜10秒のランダムな寿命を設定
                LifetimeComponent life{};
                life.maxLifetime = 5.0f + (static_cast<float>(rand()) / RAND_MAX) * 5.0f;
                m_registry.AddComponent<LifetimeComponent>(entity, life);
            }
        }
    }

    ImGui::End();

    // ECS 状態のビューワー描画
    m_debugViewer->DrawWindow(m_registry);

    // デバッグカメラの更新
    if (m_debugCamera)
    {
        m_debugCamera->Update();
    }

    // --- メインループの決定的なパイプライン（順序） ---

    // 1. 各エンティティのAIやローカル位置の更新
    EnemyBehaviorSystem::Update(m_registry, deltaTime);

    // 2. 親子関係の解決（LocalからWorldMatrixの計算）
    HierarchySystem::Update(m_registry);

    // 3. 寿命判定（尽きたものはDeferredQueueへ）
    LifetimeSystem::Update(m_registry, deltaTime);

    // 4. フレーム末尾の破棄実行
    m_registry.FlushGarbageCollection();
}

void EcsDebugScene::Draw3D()
{
    // Draw3Dでは親クラスの描画も必要なら呼ぶ（ECS以外を描画する場合）
    BaseScene::Draw3D();
    
    if (sceneManager_ && m_instancedRenderer)
    {
        Camera* camera = sceneManager_->GetCameraManager()->GetActiveCamera();
        LightManager* lightManager = sceneManager_->GetLightManager();
        ShadowMapManager* shadowMapManager = sceneManager_->GetShadowMapManager();

        // Component単位の描画群
        InstancedRenderSystem::Draw(m_registry, *m_instancedRenderer, camera, lightManager, shadowMapManager);
    }
}

void EcsDebugScene::Draw2D()
{
}
