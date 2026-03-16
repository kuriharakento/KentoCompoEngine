#include "LifetimeSystem.h"

void LifetimeSystem::Update(Registry& registry, float deltaTime)
{
    auto lifetimeView = registry.View<LifetimeComponent>();
    if (!lifetimeView) return;

    // 全要素をなぞる（Sparse SetのDense配列に対する最速ループ）
    for (uint32_t i = 0; i < lifetimeView->GetSize(); ++i)
    {
        EntityID entity = lifetimeView->GetEntityFromDenseIndex(i);
        LifetimeComponent& lifetime = lifetimeView->GetData(entity);

        lifetime.currentAge += deltaTime;

        // 寿命が尽きたら破棄を予約する（ここでは即時削除しないのでイテレータは壊れない）
        if (lifetime.currentAge >= lifetime.maxLifetime)
        {
            registry.DestroyEntityDeferred(entity);
        }
    }
}
