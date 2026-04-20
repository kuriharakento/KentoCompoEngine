#pragma once

#include <vector>
#include <unordered_set>
#include "engine/ecs/Entity.h"

/**
 * @brief 衝突イベントと状態を保持するコンポーネント。
 */
struct CollisionResponseComponent
{
    // 現在このフレームで衝突しているエンティティ
    std::unordered_set<EntityID> currentCollisions_;
    
    // 前フレームで衝突していたエンティティ（Exit判定用）
    std::unordered_set<EntityID> previousCollisions_;

    // このフレームで発生したイベント（必要に応じてクリアされる）
    std::vector<EntityID> enteredEntities_;
    std::vector<EntityID> stayedEntities_;
    std::vector<EntityID> exitedEntities_;

    void ClearFrameEvents()
    {
        previousCollisions_ = std::move(currentCollisions_);
        currentCollisions_.clear();
        enteredEntities_.clear();
        stayedEntities_.clear();
        exitedEntities_.clear();
    }
};
