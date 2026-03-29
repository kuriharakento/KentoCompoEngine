#include "EnemySpawnSystem.h"
#include "engine/ecs/Registry.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/TagComponent.h" // EcsTagComponent
#include "engine/ecs/components/EnemyAIComponent.h"
#include "application/ecs/components/StatusComponent.h"
#include "application/ecs/components/ImpactChargeComponent.h"
#include "engine/ecs/components/InstancedRenderComponent.h"
#include "engine/ecs/components/ColliderComponent.h"
#include "engine/ecs/components/CollisionResponseComponent.h"
#include "application/ecs/components/EnemyTypeComponent.h"
#include "application/ecs/CollisionConfig.h"
#include "engine/time/TimeManager.h"
#include "math/MathUtils.h"

// 必要なマネージャーのヘッダーを明示
#include "engine/manager/scene/CameraManager.h"
#include "engine/graphics/3d/Object3dCommon.h"
#include "engine/manager/scene/LightManager.h"

#include <random>

void EnemySpawnSystem::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, CameraManager* cameraManager)
{
    object3dCommon_ = object3dCommon;
    lightManager_ = lightManager;
    cameraManager_ = cameraManager;
}

void EnemySpawnSystem::Update(Registry& registry)
{
    float dt = TimeManager::GetInstance().GetGameContext().deltaTime;
    if (dt <= 0.0f) dt = 0.0166f;

    elapsedTime_ += dt;
    spawnTimer_ += dt;

    // 時間経過に応じたスポーンレートの増加 (指数関数的、または時間比例)
    float currentSpawnRate = spawnRate_ * (1.0f + elapsedTime_ * 0.05f);
    float spawnInterval = 1.0f / (currentSpawnRate > 0.1f ? currentSpawnRate : 0.1f);

    if (spawnTimer_ >= spawnInterval)
    {
        spawnTimer_ = 0.0f;
        SpawnEnemy(registry);
    }
}

void EnemySpawnSystem::SpawnEnemy(Registry& registry)
{
    // プレイヤーの位置を取得 (ecs::EcsTagComponent::Type::Player で探す)
    Vector3 playerPos = { 0, 0, 0 };
    auto tagView = registry.View<ecs::EcsTagComponent>();
    if (tagView)
    {
        for (uint32_t i = 0; i < tagView->GetSize(); ++i)
        {
            if (tagView->GetDataFromDenseIndex(i).type == ecs::EcsTagComponent::Type::Player)
            {
                EntityID player = tagView->GetEntityFromDenseIndex(i);
                if (registry.HasComponent<TransformComponent>(player))
                {
                    playerPos = registry.GetComponent<TransformComponent>(player).localPosition_;
                }
                break;
            }
        }
    }

    // ランダムな位置を算出 (円環状)
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> angleDist(0, 2.0f * 3.14159f);
    std::uniform_real_distribution<float> radiusDist(innerRadius_, outerRadius_);

    float angle = angleDist(gen);
    float radius = radiusDist(gen);
    Vector3 spawnOffset = { std::cos(angle) * radius, 0, std::sin(angle) * radius };
    Vector3 spawnPos = playerPos + spawnOffset;

    // Entity作成
    EntityID enemy = registry.CreateEntity();
    
    ecs::EcsTagComponent tag;
    tag.type = ecs::EcsTagComponent::Type::Enemy;
    registry.AddComponent<ecs::EcsTagComponent>(enemy, tag);

    registry.AddComponent<TransformComponent>(enemy, { spawnPos, {0,0,0}, {1,1,1} });
    
    // Status
    ecs::StatusComponent status;
    status.hp_.SetBase(10.0f + elapsedTime_ * 0.1f); // 時間とともに硬くなる
    status.moveSpeed_.SetBase(3.5f + static_cast<float>(rand() % 100) / 100.0f * 1.0f);
    registry.AddComponent<ecs::StatusComponent>(enemy, std::move(status));

    // AI
    EnemyAIComponent ai;
    ai.targetEntity_ = kInvalidEntity; 
    registry.AddComponent<EnemyAIComponent>(enemy, std::move(ai));

    // Impact Charge (誘爆用)
    registry.AddComponent<ImpactChargeComponent>(enemy, {});

    // Rendering (Instanced)
    InstancedRenderComponent render;
    render.modelName_ = "enemy"; 
    registry.AddComponent<InstancedRenderComponent>(enemy, render);

    // Collider
    ColliderComponent col;
    col.type_ = ColliderType::Sphere;
    col.sphere_.radius = 1.0f;
    col.previousPosition_ = spawnPos;
    
    // フィルタリング設定
    col.layer = CollisionLayer::Enemy;
    col.mask = CollisionLayer::Player | CollisionLayer::PlayerBullet;

    // 衝突応答
    col.onCollisionEnter = [&registry, enemy](const CollisionPartnerInfo& other) {
        if (registry.HasComponent<ColliderComponent>(other.entity)) {
            auto& otherCol = registry.GetComponent<ColliderComponent>(other.entity);
            
            // プレイヤーの弾に当たったら、自分（敵）を消す
            if (otherCol.layer & CollisionLayer::PlayerBullet) {
                registry.DestroyEntityDeferred(enemy);
            }
            // プレイヤーに当たったら、ダメージを与えて自分（敵）を消す
            else if (otherCol.layer & CollisionLayer::Player) {
                if (registry.HasComponent<ecs::StatusComponent>(other.entity)) {
                    auto& status = registry.GetComponent<ecs::StatusComponent>(other.entity);
                    float currentHp = status.hp_.GetBase();
                    status.hp_.SetBase(currentHp - 1.0f); // 暫定1ダメージ
                }
                registry.DestroyEntityDeferred(enemy);
            }
        }
    };
    registry.AddComponent<ColliderComponent>(enemy, col);
    registry.AddComponent<CollisionResponseComponent>(enemy, {});

    // [New] 敵の種別（近接型をデフォルトとして追加）
    EnemyTypeComponent typeComp;
    typeComp.type = EnemyType::Melee;
    registry.AddComponent<EnemyTypeComponent>(enemy, typeComp);
}
