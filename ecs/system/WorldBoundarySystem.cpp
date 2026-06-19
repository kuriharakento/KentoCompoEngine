#include "WorldBoundarySystem.h"
#include "engine/ecs/Registry.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/WorldBoundaryComponent.h"
#include <cmath>

using namespace ecs;


void WorldBoundarySystem::Update(Registry& registry)
{
    auto view = registry.View<WorldBoundaryComponent>();
    if (!view) return;

    for (uint32_t i = 0; i < view->GetSize(); ++i)
    {
        EntityID entity = view->GetEntityFromDenseIndex(i);
        if (!registry.HasComponent<TransformComponent>(entity)) continue;

        const auto& boundary = view->GetData(entity);
        if (!boundary.active_) continue;

        auto& transform = registry.GetComponent<TransformComponent>(entity);
        
        // XZ平面での円形制限
        float distSq = transform.localPosition_.x * transform.localPosition_.x + 
                       transform.localPosition_.z * transform.localPosition_.z;

        if (distSq > boundary.radius_ * boundary.radius_)
        {
            float dist = std::sqrt(distSq);
            if (dist > 0.0001f)
            {
                transform.localPosition_.x = (transform.localPosition_.x / dist) * boundary.radius_;
                transform.localPosition_.z = (transform.localPosition_.z / dist) * boundary.radius_;
                
                // 座標が変わったので Dirty フラグを立てる
                transform.isDirty_ = true;
            }
        }
    }
}

