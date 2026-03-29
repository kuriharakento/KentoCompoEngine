#pragma once

#include "engine/gameobject/component/base/ICollisionComponent.h" // ColliderType 利用
#include "math/AABB.h"
#include "math/OBB.h"
#include "math/Sphere.h"
#include "math/Vector3.h"
#include "engine/ecs/Entity.h"
#include <functional>

struct CollisionPartnerInfo
{
    EntityID entity; // 衝突した相手のID
};

/**
 * @brief 衝突判定の形状とデータを保持するコンポーネント。
 */
struct ColliderComponent
{
    // --- フィルタリング設定 ---
    uint32_t layer = 0;              // 自身が属するレイヤー（CollisionLayer::Player 等）
    uint32_t mask  = 0xFFFFFFFF;     // 衝突を検知したい相手のレイヤーマスク

    // --- 形状データ ---
    ColliderType type_ = ColliderType::AABB;
    AABB aabb_;
    OBB obb_;
    Sphere sphere_;

    // 形状データ（ワールド座標更新後・読み取り用）
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

    // --- コールバック ---
    using CollisionCallback = std::function<void(const CollisionPartnerInfo& other)>;
    CollisionCallback onCollisionEnter; // 衝突開始時
    CollisionCallback onCollisionStay;  // 衝突継続中（毎フレーム）
    CollisionCallback onCollisionExit;  // 衝突終了時
};
