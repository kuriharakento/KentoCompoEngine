#include "LifetimeSystem.h"
#include "engine/time/TimeManager.h"

void LifetimeSystem::Update(Registry& registry)
{
    float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;
    auto lifetimeView = registry.View<LifetimeComponent>();
    if (!lifetimeView)
    {
        return;
    }

    for (uint32_t i = 0; i < lifetimeView->GetSize(); ++i)
    {
        EntityID entity = lifetimeView->GetEntityFromDenseIndex(i);
        LifetimeComponent& lifetime = lifetimeView->GetData(entity);

        lifetime.currentAge_ += deltaTime;

        if (lifetime.currentAge_ >= lifetime.maxLifetime_)
        {
            registry.DestroyEntityDeferred(entity);
        }
    }
}
