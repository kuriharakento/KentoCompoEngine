#pragma once
#include "application/GameObject/component/base/IGameObjectComponent.h"
#include "math/Vector3.h"
#include <vector>
#include <memory>
#include <random>

#include "application/GameObject/Combatable/character/enemy/base/Node/BehaviorTree/BehaviorTree.h"

class GameObject;

class AssaultEnemyBehavior : public IGameObjectComponent
{
public:
    AssaultEnemyBehavior(GameObject* target);

    void Update(GameObject* owner) override;

    void SetTarget(GameObject* target) { target_ = target; }
    void SetMoveSpeed(float speed) { moveSpeed_ = speed; }
    void SetAttackRange(float range) { attackRange_ = range; }

private:
    // 既存の行動関数
    void AimAtTarget(GameObject* owner);
    void FireWeapon(GameObject* owner);
    bool IsTargetVisible(GameObject* owner);
    bool IsInAttackRange(GameObject* owner);
    bool IsInExtendedAttackRange(GameObject* owner);
    Vector3 GetRandomStrafeDirection(GameObject* owner);
    void InitializePatrolPoints(const Vector3& centerPoint, float radius);
    Vector3 CalculateSmoothMovement(const Vector3& currentPos, const Vector3& targetPos, float maxDistance);
    float LimitMovementSpeed(float baseSpeed, float dt);
    void ForceMovement(GameObject* owner);
    bool IsStuck(GameObject* owner);

    // BTノードで使うアクション
    void IdleAction(GameObject* owner);
    void CombatAction(GameObject* owner);
    void PatrolAction(GameObject* owner);
    void RepositionAction(GameObject* owner);
    void StrafeAction(GameObject* owner);
    void RetreatAction(GameObject* owner);

    // 状態
    GameObject* target_ = nullptr;

    // 行動パラメータ
    float moveSpeed_ = 5.0f;
    float maxMoveDistancePerFrame_ = 0.3f;
    float attackRange_ = 18.0f;      // 最適射撃距離
    float minRange_ = 10.0f;         // 最小距離
    float maxRange_ = 25.0f;         // 最大距離
    float extendedMinRange_ = 8.0f;  // 拡張最小
    float extendedMaxRange_ = 25.0f; // 拡張最大
    float detectionRange_ = 35.0f;   // 検知範囲

    // 横移動用
    Vector3 strafeDirection_;
    float strafeChangeInterval_ = 1.5f;
    float strafeTendencyFactor_ = 0.5f;

    // 位置調整用
    Vector3 lastValidPosition_;
    float repositionSpeed_ = 0.0f;
    float maxRepositionSpeed_ = 1.0f;

    // パトロール用
    std::vector<Vector3> patrolPoints_;
    int currentPatrolIndex_ = 0;
    float patrolRadius_ = 20.0f;
    bool patrolInitialized_ = false;
    float patrolSpeed_ = 0.6f;

    // タイマー
    float stateTimer_ = 0.0f;
    float strafeTimer_ = 0.0f;
    float actionCooldown_ = 0.0f;
    float positionCheckTimer_ = 0.0f;

    // 動き停止検出用
    Vector3 lastPosition_;
    float stuckTimer_ = 0.0f;
    float stuckThreshold_ = 1.0f;
    bool potentiallyStuck_ = false;

    // 継続的ストレイフ制御用
    bool isStrafing_ = false;           // ストレイフ状態フラグ
    float strafeDuration_ = 3.0f;       // ストレイフ継続時間（秒）
    float strafeProbability_ = 0.25f;   // ストレイフ開始確率
    float combatStateTimer_ = 0.0f;     // 戦闘状態タイマー

    // 新しいメソッド宣言も追加
    void ContinuousStrafAction(GameObject* owner);

    // 乱数生成
    std::mt19937 rng_;

    // ビヘイビアツリー
    std::unique_ptr<BehaviorTree> behaviorTree_;

    // ツリー構築
    void BuildBehaviorTree();
};