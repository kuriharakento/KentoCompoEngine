#pragma once

// No namespaces

/**
 * @brief 敵の固有状態（ステータスやステートマシン）を保持するコンポーネント。
 */
struct EnemyStateComponent
{
    // 敵のAIステート定義
    enum class State
    {
        Idle,   // 待機
        Move,   // 移動
        Attack, // 攻撃
        Dead    // 死亡（演出中など、即時破棄せず残す場合）
    };

    // 現在のヒットポイント
    int hp = 100;

    // 現在進行中のAIステート
    State currentState = State::Idle;
    
    // ステート開始からの経過時間（アニメーションや攻撃間隔の判定用）
    float stateTimer = 0.0f;
};


