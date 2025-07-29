#pragma once
#include "application/GameObject/component/base/IGameObjectComponent.h"
#include "math/Vector3.h"
#include <vector>
#include <random>

class GameObject;

class AssaultEnemyBehavior : public IGameObjectComponent
{
public:
    // コンストラクタ
    AssaultEnemyBehavior(GameObject* target);

    // 基本インターフェース
    void Update(GameObject* owner) override;

    // パラメータ設定
    void SetTarget(GameObject* target) { target_ = target; }
    void SetMoveSpeed(float speed) { moveSpeed_ = speed; }
    void SetAttackRange(float range) { attackRange_ = range; }

private:
    enum class State
    {
        Idle,       // 待機状態
        Combat,     // 戦闘状態
        Patrol,     // 巡回状態
        Reposition, // 位置調整
        Strafe,     // 横移動
        Retreat     // 後退
    };

    // 各状態の行動
    void IdleBehavior(GameObject* owner);
    void CombatBehavior(GameObject* owner);
    void PatrolBehavior(GameObject* owner);
    void RepositionBehavior(GameObject* owner);
    void StrafeBehavior(GameObject* owner);
    void RetreatBehavior(GameObject* owner);

    // 補助メソッド
    void AimAtTarget(GameObject* owner);
    void FireWeapon(GameObject* owner);
    bool IsTargetVisible(GameObject* owner);
    bool IsInAttackRange(GameObject* owner);
    bool IsInExtendedAttackRange(GameObject* owner);
    Vector3 GetRandomStrafeDirection(GameObject* owner);
    void InitializePatrolPoints(const Vector3& centerPoint, float radius);

    // 移動計算用のヘルパー
    Vector3 CalculateSmoothMovement(const Vector3& currentPos, const Vector3& targetPos, float maxDistance);
    float LimitMovementSpeed(float baseSpeed, float dt);
    void ForceMovement(GameObject* owner); // 動きが止まった時の強制移動用

    // 動きの停止検出
    bool IsStuck(GameObject* owner);

    // メンバ変数
    State currentState_ = State::Idle;
    GameObject* target_ = nullptr;

    // タイマー
    float stateTimer_ = 0.0f;
    float strafeTimer_ = 0.0f;
    float actionCooldown_ = 0.0f;
    float positionCheckTimer_ = 0.0f;

    // 動き停止検出用
    Vector3 lastPosition_;
    float stuckTimer_ = 0.0f;
    float stuckThreshold_ = 1.0f; // 1秒間動きがなければ停止と判断
    bool potentiallyStuck_ = false;

    // 移動パラメータ
    float moveSpeed_ = 2.0f;
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
	float maxRepositionSpeed_ = 1.0f; // 最大リポジション速度

    // 巡回用
    std::vector<Vector3> patrolPoints_;
    int currentPatrolIndex_ = 0;
    float patrolRadius_ = 20.0f;
    bool patrolInitialized_ = false;
    float patrolSpeed_ = 0.6f;

    // 乱数生成
    std::mt19937 rng_;
};