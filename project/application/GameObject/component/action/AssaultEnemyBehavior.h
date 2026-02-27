#pragma once
#include "math/Vector3.h"
#include <vector>
#include <memory>
#include <string>
#include <random>
#include "engine/gameobject/component/collision/RayColliderComponent.h"
#include "application/gameObject/combatable/character/enemy/base/Node/BehaviorTree/BehaviorTree.h"
#include "engine/gameobject/component/base/IActionComponent.h"
#include "engine/gameobject/component/collision/CollisionAlgorithm.h"

/**
 * @brief アサルトライフルを持つ敵のAI行動コンポーネント
 *
 * ビヘイビアツリーを使用して、パトロール、戦闘、追跡などの行動を管理する
 */
class AssaultEnemyBehavior : public IActionComponent
{
public:
    /**
     * @brief コンストラクタ
     * @param target 追跡対象のゲームオブジェクト
     */
    AssaultEnemyBehavior(GameObject* target);
    ~AssaultEnemyBehavior() = default;

    /**
     * @brief 初期化処理
     * @param owner このコンポーネントをアタッチした GameObject
     */
    void Initialize(GameObject* owner);

    /**
     * @brief 毎フレームの更新処理
     * @param owner このコンポーネントをアタッチした GameObject
     */
    void Update(GameObject* owner) override;

    /**
     * @brief ターゲットを設定する
     * @param target 追跡対象のゲームオブジェクト
     */
    void SetTarget(GameObject* target) { target_ = target; }

    /**
     * @brief 移動速度を設定する
     * @param speed 移動速度
     */
    void SetMoveSpeed(float speed) { moveSpeed_ = speed; }

    /**
     * @brief 攻撃範囲を設定する
     * @param range 攻撃範囲
     */
    void SetAttackRange(float range) { attackRange_ = range; }

private:
    // 行動パラメータの定数
    static constexpr float kDefaultMoveSpeed = 5.0f;
    static constexpr float kDefaultMaxMoveDistancePerFrame = 0.3f;
    static constexpr float kDefaultAttackRange = 18.0f;
    static constexpr float kDefaultMinRange = 10.0f;
    static constexpr float kDefaultMaxRange = 25.0f;
    static constexpr float kDefaultExtendedMinRange = 8.0f;
    static constexpr float kDefaultExtendedMaxRange = 25.0f;
    static constexpr float kDefaultDetectionRange = 35.0f;
    static constexpr float kDefaultStrafeChangeInterval = 1.5f;
    static constexpr float kDefaultStrafeTendencyFactor = 0.5f;
    static constexpr float kDefaultMaxRepositionSpeed = 1.0f;
    static constexpr float kDefaultPatrolRadius = 20.0f;
    static constexpr float kDefaultPatrolSpeed = 0.6f;
    static constexpr float kDefaultStuckThreshold = 1.0f;
    static constexpr float kDefaultStrafeDuration = 3.0f;
    static constexpr float kDefaultStrafeProbability = 0.25f;
    static constexpr float kStrafeSpeedMultiplier = 1.3f;
    static constexpr float kShootIntervalDuringStrafe = 0.2f;
    static constexpr float kPatrolArrivalThreshold = 1.5f;
    static constexpr float kRetreatSpeedMultiplier = 1.2f;
    static constexpr float kStrafeActionSpeedMultiplier = 0.6f;
    static constexpr float kRepositionAcceleration = 0.05f;
    static constexpr float kForceMovementSpeedMultiplier = 0.5f;
    static constexpr float kStuckMovementThreshold = 0.01f;
    static constexpr float kDistanceFactorAdjustment = 0.3f;
    static constexpr int kPatrolPointCount = 8;
    static constexpr float kSpawnDuration = 2.0f; // スポーン後の待機時間
    static constexpr float kFlankDuration = 4.0f; // 回り込み継続時間
    static constexpr float kFlankSpeedMultiplier = 1.0f; // 回り込み速度係数

    // ターゲットに照準を合わせる
    void AimAtTarget(GameObject* owner);
    // 武器を発射する
    void FireWeapon(GameObject* owner);
    // ターゲットが視界内にいるか確認
    bool IsTargetVisible(GameObject* owner);
    // 障害物による視線遮断を確認する（レイキャスト代用）
    bool CheckLineOfSight(GameObject* owner, const Vector3& targetPos);
    // 攻撃範囲内にいるか確認
    bool IsInAttackRange(GameObject* owner);
    // 拡張攻撃範囲内にいるか確認
    bool IsInExtendedAttackRange(GameObject* owner);
    // ランダムな横移動方向を取得
    Vector3 GetRandomStrafeDirection(GameObject* owner);
    // パトロールポイントを初期化
    void InitializePatrolPoints(const Vector3& centerPoint, float radius);
    // スムーズな移動を計算
    Vector3 CalculateSmoothMovement(const Vector3& currentPos, const Vector3& targetPos, float maxDistance);
    // 移動速度を制限
    float LimitMovementSpeed(float baseSpeed, float dt);
    // 強制移動（スタック解消用）
    void ForceMovement(GameObject* owner);
    // スタック状態かどうか確認
    bool IsStuck(GameObject* owner);

    // BTノードで使うアクション
    // 待機行動
    void IdleAction(GameObject* owner);
    // パトロール行動
    void PatrolAction(GameObject* owner);
    // 位置調整行動
    void RepositionAction(GameObject* owner);
    // 横移動行動
    void StrafeAction(GameObject* owner);
    // 後退行動
    void RetreatAction(GameObject* owner);
    // 回り込み行動
    void FlankAction(GameObject* owner);

    // 追跡対象
    GameObject* target_ = nullptr;

    // 移動速度
    float moveSpeed_ = kDefaultMoveSpeed;
    // 1フレームあたりの最大移動距離
    float maxMoveDistancePerFrame_ = kDefaultMaxMoveDistancePerFrame;
    // 最適射撃距離
    float attackRange_ = kDefaultAttackRange;
    // 最小距離
    float minRange_ = kDefaultMinRange;
    // 最大距離
    float maxRange_ = kDefaultMaxRange;
    // 拡張最小距離
    float extendedMinRange_ = kDefaultExtendedMinRange;
    // 拡張最大距離
    float extendedMaxRange_ = kDefaultExtendedMaxRange;
    // 検知範囲
    float detectionRange_ = kDefaultDetectionRange;

    // 横移動方向
    Vector3 strafeDirection_;
    // 横移動方向変更間隔
    float strafeChangeInterval_ = kDefaultStrafeChangeInterval;
    // 横移動傾向係数
    float strafeTendencyFactor_ = kDefaultStrafeTendencyFactor;

    // 最後の有効位置
    Vector3 lastValidPosition_;
    // 位置調整速度
    float repositionSpeed_ = 0.0f;
    // 最大位置調整速度
    float maxRepositionSpeed_ = kDefaultMaxRepositionSpeed;

    // パトロールポイントのリスト
    std::vector<Vector3> patrolPoints_;
    // 現在のパトロールインデックス
    int currentPatrolIndex_ = 0;
    // パトロール半径
    float patrolRadius_ = kDefaultPatrolRadius;
    // パトロール初期化フラグ
    bool patrolInitialized_ = false;
    // パトロール速度係数
    float patrolSpeed_ = kDefaultPatrolSpeed;

    // 状態タイマー
    float stateTimer_ = 0.0f;
    // 横移動タイマー
    float strafeTimer_ = 0.0f;
    // 行動クールダウン
    float actionCooldown_ = 0.0f;
    // 位置確認タイマー
    float positionCheckTimer_ = 0.0f;
    // スポーン待機タイマー
    float spawnTimer_ = 0.0f;

    // 最後の位置（スタック検出用）
    Vector3 lastPosition_;
    // スタック検出タイマー
    float stuckTimer_ = 0.0f;
    // スタック判定閾値
    float stuckThreshold_ = kDefaultStuckThreshold;
    // スタック可能性フラグ
    bool potentiallyStuck_ = false;

    // ストレイフ状態フラグ
    bool isStrafing_ = false;
    // ストレイフ継続時間（秒）
    float strafeDuration_ = kDefaultStrafeDuration;
    // ストレイフ開始確率
    float strafeProbability_ = kDefaultStrafeProbability;
    // 戦闘状態タイマー
    float combatStateTimer_ = 0.0f;

    // 回り込み（Flanking）管理
    bool isFlanking_ = false;
    float flankTimer_ = 0.0f;
    // -1 (左) or 1 (右)
    float flankDirectionSign_ = 1.0f; 

    // 継続的なストレイフ行動
    void ContinuousStrafAction(GameObject* owner);

    // 乱数生成器
    std::mt19937 rng_;

    // ビヘイビアツリー
    std::unique_ptr<BehaviorTree> behaviorTree_;

    // ビヘイビアツリーを構築
    void BuildBehaviorTree();
	// 視線遮断フラグ（RayColliderのコールバックで更新）
	bool isSightBlocked_ = false;

	// レイ判定用オブジェクト
	std::unique_ptr<GameObject> sightRayObject_;
	RayColliderComponent* rayCollider_ = nullptr;
};