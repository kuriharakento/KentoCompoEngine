#pragma once

#include "engine/gameobject/component/base/ICollisionComponent.h" // ColliderType 利用
#include "math/AABB.h"
#include "math/OBB.h"
#include "math/Vector3.h"

/**
 * @brief 衝突判定の形状とデータを保持するコンポーネント。
 */
struct ColliderComponent
{
    // コライダーの種類
    ColliderType type_ = ColliderType::AABB;
    
    // 形状データ（Union または個別保持。メモリ効率のため AABB を基本とする）
    AABB aabb_;
    OBB obb_;
    
    // 中心からのオフセット
    Vector3 offset_ = { 0.0f, 0.0f, 0.0f };
    
    // トリガー判定（物理的な押し返しを行わない判定）
    bool isTrigger_ = false;
    
    // 有効フラグ
    bool isActive_ = true;
};
