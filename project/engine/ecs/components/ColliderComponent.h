#pragma once

#include "engine/gameobject/component/base/ICollisionComponent.h" // ColliderType 利用
#include "math/AABB.h"
#include "math/OBB.h"
#include "math/Sphere.h"
#include "math/Vector3.h"
#include "engine/ecs/Entity.h"
#include <functional>

/**
 * @brief 衝突判定の形状とデータを保持するコンポーネント。
 */
struct ColliderComponent
{
    // コライダーの種類
    ColliderType type_ = ColliderType::AABB;
    
    // 形状データ（ローカル・サイズ設定用）
    AABB aabb_;
    OBB obb_;
    Sphere sphere_;

    // 形状データ（ワールド・毎フレーム更新・読み取り用）
    AABB worldAabb_;
    OBB worldObb_;
    Sphere worldSphere_;
    
    // 中心からのオフセット
    Vector3 offset_ = { 0.0f, 0.0f, 0.0f };
    
    // トリガー判定（物理的な押し返しを行わない判定）
    bool isTrigger_ = false;
    
    // 有効フラグ
    bool isActive_ = true;

    // すり抜け防止機能（サブステップ方式）を使用するか
    bool useSubstep_ = false;

    // 前フレームのワールド座標（すり抜け防止カウント用）
    Vector3 previousPosition_ = { 0.0f, 0.0f, 0.0f };

    // 衝突時のコールバック
    std::function<void(EntityID other)> onEnter_;
    std::function<void(EntityID other)> onStay_;
    std::function<void(EntityID other)> onExit_;
};
