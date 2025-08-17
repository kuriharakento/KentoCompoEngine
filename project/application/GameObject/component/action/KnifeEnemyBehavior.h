#pragma once
#include "application/GameObject/component/base/IGameObjectComponent.h"
#include "math/Vector3.h"
#include <vector>
#include <memory>
#include <random>
#include "application/GameObject/Combatable/character/enemy/base/Node/BehaviorTree/BehaviorTree.h"

class GameObject;

class KnifeEnemyBehavior : public IGameObjectComponent
{
public:
    KnifeEnemyBehavior(GameObject* target, GameObject* knife);

    void Update(GameObject* owner) override;

    void SetTarget(GameObject* target) { target_ = target; }
    void SetMoveSpeed(float speed) { moveSpeed_ = speed; }
    void SetAttackRange(float range) { attackRange_ = range; }

private:
    void BuildBehaviorTree();

    // BTアクション
    void IdleAction(GameObject* owner);
    bool PatrolAction(GameObject* owner);  // ← bool返却
    bool ChaseAction(GameObject* owner);   // ← bool返却
    bool AttackAction(GameObject* owner);  // ← bool返却

    // 補助
    bool IsTargetVisible(GameObject* owner);
    bool IsInAttackRange(GameObject* owner);
    void InitializePatrolPoints(const Vector3& centerPoint, float radius);
    float LimitMovementSpeed(float baseSpeed, float dt);

    // 状態
    GameObject* target_ = nullptr;
    GameObject* knife_ = nullptr;

    float moveSpeed_ = 4.5f;
    float attackRange_ = 3.0f;    // 近接は短め
    float detectionRange_ = 25.0f;

    // パトロール
    std::vector<Vector3> patrolPoints_;
    int currentPatrolIndex_ = 0;
    float patrolRadius_ = 14.0f;
    bool patrolInitialized_ = false;
    float patrolSpeed_ = 0.6f;

    // タイマー
    float attackCooldown_ = 0.0f;
    float attackInterval_ = 1.4f;  // 攻撃間隔

    std::mt19937 rng_;

    // BT
    std::unique_ptr<BehaviorTree> behaviorTree_;
};