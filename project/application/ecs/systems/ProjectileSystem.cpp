#include "ProjectileSystem.h"
#include "engine/ecs/Registry.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/TagComponent.h" // ecs::TagComponent
#include "application/ecs/components/ProjectileComponent.h"
#include "engine/time/TimeManager.h"
#include "application/effect/BulletTrailManager.h"

void ProjectileSystem::Update(Registry& registry)
{
    float dt = TimeManager::GetInstance().GetGameContext().deltaTime;
    if (dt <= 0.0f) dt = 0.0166f;

    auto view = registry.View<ProjectileComponent>();
    if (!view) return;

    for (uint32_t i = 0; i < view->GetSize(); ++i)
    {
        EntityID entity = view->GetEntityFromDenseIndex(i);
        if (!registry.HasComponent<TransformComponent>(entity)) continue;

        ProjectileComponent& pc = view->GetDataFromDenseIndex(i);
        TransformComponent& trans = registry.GetComponent<TransformComponent>(entity);

        // 移動
        trans.localPosition_ = trans.localPosition_ + pc.velocity_ * dt;
        trans.isDirty_ = true;

        // 子弾・軌跡の更新 (BulletTrailManager)
        if (pc.trailId_ != -1)
        {
            BulletTrailManager::GetInstance().UpdateBulletManual(pc.trailId_, trans.localPosition_);
        }

        // 寿命
        pc.lifetime_ -= dt;
        if (pc.lifetime_ <= 0.0f)
        {
            if (pc.trailId_ != -1)
            {
                BulletTrailManager::GetInstance().UnregisterBullet(pc.trailId_);
                pc.trailId_ = -1;
            }
            registry.DestroyEntityDeferred(entity);
        }
    }
}
