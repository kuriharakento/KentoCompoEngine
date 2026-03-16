#pragma once

#include "../../../engine/ecs/Entity.h"

// No namespaces

/**
 * @brief エンティティ間の親子関係を表現する。
 * 
 * ポインタを使わずEntityIDのリンクリスト構造にすることで、メモリ連続性を保つ。
 */
struct EnemyStateComponent
{
    // 敵のAIステート定義
    enum class State
    {
        Idle,   // 待機
        Move,   // 移動
        Attack, // 攻撃
        Dead    // 死亡
    };

    // ヒットポイント
    int hp_ = 100;

    // 現在のAIステート
    State currentState_ = State::Idle;
    
    // ステート開始からの経過時間
    float stateTimer_ = 0.0f;
};
