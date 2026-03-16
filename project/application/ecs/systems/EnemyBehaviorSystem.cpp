#include "EnemyBehaviorSystem.h"

void EnemyBehaviorSystem::Update(Registry& registry, float deltaTime)
{
    // 敵の「状態」と「位置」の両方を持っているEntityを対象にする
    auto stateView = registry.View<EnemyStateComponent>();
    if (!stateView) return;

    for (uint32_t i = 0; i < stateView->GetSize(); ++i)
    {
        EntityID entity = stateView->GetEntityFromDenseIndex(i);

        // 必須コンポーネント（Transform）を持っているかチェック
        if (!registry.HasComponent<TransformComponent>(entity)) continue;

        EnemyStateComponent& state = stateView->GetData(entity);
        TransformComponent& transform = registry.GetComponent<TransformComponent>(entity);

        // ステート滞在時間を更新
        state.stateTimer += deltaTime;

        // --- ステートマシン処理（AIロジック） ---
        switch (state.currentState)
        {
        case EnemyStateComponent::State::Idle:
            // 例: 1秒待機したら移動ステートへ移行
            if (state.stateTimer > 1.0f) {
                state.currentState = EnemyStateComponent::State::Move;
                state.stateTimer = 0.0f;
            }
            break;

        case EnemyStateComponent::State::Move:
            // 例: ローカル座標のZ方向へ前進する
            transform.localPosition.z += 5.0f * deltaTime; // 適当な速度
            
            // 例: 3秒進んだら攻撃ステートへ移行
            if (state.stateTimer > 3.0f) {
                state.currentState = EnemyStateComponent::State::Attack;
                state.stateTimer = 0.0f;
            }
            break;

        case EnemyStateComponent::State::Attack:
            // 例: 攻撃演出を0.5秒行ったらIdleに戻る
            if (state.stateTimer > 0.5f) {
                state.currentState = EnemyStateComponent::State::Idle;
                state.stateTimer = 0.0f;
            }
            break;

        case EnemyStateComponent::State::Dead:
            // ECSの LifetimeSystem が無い場合はここで破棄予約を行ってもよい。
            // registry.DestroyEntityDeferred(entity);
            break;
        }
    }
}
