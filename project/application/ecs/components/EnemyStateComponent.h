#pragma once

/**
 * @brief 敵の固有状態（ステータスやステートマシン）を保持するコンポーネント。
 */
struct EnemyStateComponent
{
    /**
     * @brief 敵のAIステート定義
     */
    enum class State
    {
        Idle,   // 待機
        Move,   // 移動
        Attack, // 攻撃
        Dead    // 死亡
    };

    // 現在のヒットポイント
    int hp_ = 100;

    // 現在進行中のAIステート
    State currentState_ = State::Idle;
    
    // ステート開始からの経過時間
    float stateTimer_ = 0.0f;
};
