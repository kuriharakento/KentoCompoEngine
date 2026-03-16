#include "EnemyBehaviorSystem.h"

void EnemyBehaviorSystem::Update(Registry& registry, float deltaTime)
{
    auto stateView = registry.View<EnemyStateComponent>();
    if (!stateView)
    {
        return;
    }

    for (uint32_t i = 0; i < stateView->GetSize(); ++i)
    {
        EntityID entity = stateView->GetEntityFromDenseIndex(i);

        if (!registry.HasComponent<TransformComponent>(entity))
        {
            continue;
        }

        EnemyStateComponent& state = stateView->GetData(entity);
        TransformComponent& transform = registry.GetComponent<TransformComponent>(entity);

        // ステート滞在時間を更新
        state.stateTimer_ += deltaTime;

        // --- ステートマシン処理 ---
        switch (state.currentState_)
        {
        case EnemyStateComponent::State::Idle:
            // 1秒待機したら移動へ移行
            if (state.stateTimer_ > 1.0f)
            {
                state.currentState_ = EnemyStateComponent::State::Move;
                state.stateTimer_ = 0.0f;
            }
            break;

        case EnemyStateComponent::State::Move:
            // ローカル座標のZ方向へ前進
            transform.localPosition_.z += 5.0f * deltaTime;
            
            // 3秒進んだら攻撃へ移行
            if (state.stateTimer_ > 3.0f)
            {
                state.currentState_ = EnemyStateComponent::State::Attack;
                state.stateTimer_ = 0.0f;
            }
            break;

        case EnemyStateComponent::State::Attack:
            // 0.5秒攻撃したらIdleに戻る
            if (state.stateTimer_ > 0.5f)
            {
                state.currentState_ = EnemyStateComponent::State::Idle;
                state.stateTimer_ = 0.0f;
            }
            break;

        case EnemyStateComponent::State::Dead:
            break;
        }
    }
}
