#include "EcsStatusSystem.h"
#include "engine/ecs/Registry.h"
#include "application/ecs/components/StatusComponent.h"
#include "engine/time/TimeManager.h"

using namespace ecs;


void EcsStatusSystem::Update(Registry& registry)
{
    float dt = TimeManager::GetInstance().GetGameContext().deltaTime;
    
    auto view = registry.View<ecs::StatusComponent>();
    if (!view) return;

    for (uint32_t i = 0; i < view->GetSize(); ++i)
    {
        auto& status = view->GetDataFromDenseIndex(i);
        
        // StatusValueのバフタイマーなどを更新
        status.hp_.Update(dt);
        status.maxHp_.Update(dt);
        status.attackPower_.Update(dt);
        status.moveSpeed_.Update(dt);
        
        // 死亡判定
        if (status.hp_.GetValue() <= 0.0f)
        {
            status.isAlive_ = false;
        }
    }
}

