#include "AssaultEnemyBehavior.h"
#include "AssaultRifleComponent.h"
#include "application/GameObject/base/GameObject.h"
#include "math/MathUtils.h"
#include "time/TimeManager.h"
#include <cmath>
#include <algorithm>
#include <random>

#include "application/GameObject/Combatable/character/enemy/base/Node/ActionNode.h"
#include "application/GameObject/Combatable/character/enemy/base/Node/ConditionNode.h"
#include "application/GameObject/Combatable/character/enemy/base/Node/SelectorNode.h"
#include "application/GameObject/Combatable/character/enemy/base/Node/SequenceNode.h"

// --- コンストラクタ ---
AssaultEnemyBehavior::AssaultEnemyBehavior(GameObject* target) : target_(target)
{
    std::random_device rd;
    rng_ = std::mt19937(rd());
    BuildBehaviorTree();
}

// --- Update ---
void AssaultEnemyBehavior::Update(GameObject* owner)
{
    float deltaTime = TimeManager::GetInstance().GetDeltaTime();
    stateTimer_ += deltaTime;
    strafeTimer_ += deltaTime;
    positionCheckTimer_ += deltaTime;
    combatStateTimer_ += deltaTime;  // 追加
    if (actionCooldown_ > 0) actionCooldown_ -= deltaTime;
    lastPosition_ = owner->GetPosition();

    // Blackboardへ情報セット
    auto& bb = behaviorTree_->GetBlackboard();
    bb.Set<GameObject*>("Owner", owner);
    bb.Set<GameObject*>("Target", target_);
    bb.Set<Vector3>("TargetPosition", target_ ? target_->GetPosition() : Vector3());
    bb.Set<bool>("IsTargetVisible", IsTargetVisible(owner));
    bb.Set<bool>("IsInAttackRange", IsInAttackRange(owner));
    bb.Set<bool>("IsInExtendedAttackRange", IsInExtendedAttackRange(owner));
    bb.Set<Vector3>("LastValidPosition", lastValidPosition_);
    bb.Set<float>("StateTimer", stateTimer_);
    bb.Set<float>("StrafeTimer", strafeTimer_);
    bb.Set<float>("MoveSpeed", moveSpeed_);
    bb.Set<float>("AttackRange", attackRange_);
    bb.Set<float>("MinRange", minRange_);
    bb.Set<float>("MaxRange", maxRange_);
    bb.Set<float>("ExtendedMinRange", extendedMinRange_);
    bb.Set<float>("ExtendedMaxRange", extendedMaxRange_);
    bb.Set<float>("DetectionRange", detectionRange_);
    bb.Set<int>("CurrentPatrolIndex", currentPatrolIndex_);
    bb.Set<bool>("PatrolInitialized", patrolInitialized_);

    // BTで行動管理
    behaviorTree_->Tick();
}

void AssaultEnemyBehavior::ContinuousStrafAction(GameObject* owner)
{
    if (!target_)
    {
        isStrafing_ = false;
        return;
    }

    float deltaTime = TimeManager::GetInstance().GetDeltaTime();
    strafeTimer_ += deltaTime;

    // ストレイフ継続時間チェック
    if (strafeTimer_ >= strafeDuration_)
    {
        isStrafing_ = false;
        strafeTimer_ = 0.0f;
        return;
    }

    // ストレイフ方向を定期的に変更（継続中も方向転換）
    if (fmod(strafeTimer_, strafeChangeInterval_) < deltaTime)
    {
        strafeDirection_ = GetRandomStrafeDirection(owner);
    }

    // ストレイフ移動実行
    float strafeSpeed = moveSpeed_ * 1.3f;
    float moveDistance = LimitMovementSpeed(strafeSpeed, deltaTime);
    Vector3 newPosition = owner->GetPosition() + strafeDirection_ * moveDistance;
    owner->SetPosition(newPosition);

    // ストレイフ中も継続的に射撃
    if (actionCooldown_ <= 0.0f)
    {
        FireWeapon(owner);
        actionCooldown_ = 0.2f; // 射撃間隔
    }

    // エイミングも継続
    AimAtTarget(owner);
}

// --- BT構築 ---
void AssaultEnemyBehavior::BuildBehaviorTree()
{
    auto root = std::make_unique<SelectorNode>();

    // 1. スタック検知で強制移動
    auto stuckSeq = std::make_unique<SequenceNode>();
    stuckSeq->AddChild(std::make_unique<ConditionNode>([this](Blackboard& bb) {
        auto owner = bb.Get<GameObject*>("Owner");
        return IsStuck(owner);
                                                       }));
    stuckSeq->AddChild(std::make_unique<ActionNode>([this](Blackboard& bb) {
        auto owner = bb.Get<GameObject*>("Owner");
        ForceMovement(owner);
        return NodeStatus::Success;
                                                    }));
    root->AddChild(std::move(stuckSeq));

    // 2. 距離による後退
    auto retreatSeq = std::make_unique<SequenceNode>();
    retreatSeq->AddChild(std::make_unique<ConditionNode>([this](Blackboard& bb) {
        auto owner = bb.Get<GameObject*>("Owner");
        auto target = bb.Get<GameObject*>("Target");
        if (!target) return false;
        float dist = (target->GetPosition() - owner->GetPosition()).Length();
        return dist < minRange_;
                                                         }));
    retreatSeq->AddChild(std::make_unique<ActionNode>([this](Blackboard& bb) {
        auto owner = bb.Get<GameObject*>("Owner");
        RetreatAction(owner);
        return NodeStatus::Success;
                                                      }));
    root->AddChild(std::move(retreatSeq));

    // 3. 距離によるリポジション
    auto repositionSeq = std::make_unique<SequenceNode>();
    repositionSeq->AddChild(std::make_unique<ConditionNode>([this](Blackboard& bb) {
        auto owner = bb.Get<GameObject*>("Owner");
        auto target = bb.Get<GameObject*>("Target");
        if (!target) return false;
        float dist = (target->GetPosition() - owner->GetPosition()).Length();
        return dist > maxRange_;
                                                            }));
    repositionSeq->AddChild(std::make_unique<ActionNode>([this](Blackboard& bb) {
        auto owner = bb.Get<GameObject*>("Owner");
        RepositionAction(owner);
        return NodeStatus::Success;
                                                         }));
    root->AddChild(std::move(repositionSeq));

    // 4. 戦闘：ターゲットが見えて攻撃範囲内
    auto combatSeq = std::make_unique<SequenceNode>();
    combatSeq->AddChild(std::make_unique<ConditionNode>([this](Blackboard& bb) {
        return bb.Get<bool>("IsTargetVisible") && bb.Get<bool>("IsInAttackRange");
                                                        }));

    // 戦闘時のセレクター（継続的なストレイフまたは射撃）
    auto combatSelector = std::make_unique<SelectorNode>();

    // 4a. 継続的ストレイフ（一定期間継続）
    auto continuousStrafSeq = std::make_unique<SequenceNode>();
    continuousStrafSeq->AddChild(std::make_unique<ConditionNode>([this](Blackboard& bb) {
        // ストレイフ状態が継続中、または新たにストレイフを開始する条件
        if (isStrafing_)
        {
            return strafeTimer_ < strafeDuration_; // ストレイフ継続中
        }
        else
        {
            // 新たにストレイフを開始する条件
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            if (dist(rng_) < strafeProbability_ && combatStateTimer_ > 1.0f)
            {
                isStrafing_ = true;
                strafeTimer_ = 0.0f;
                combatStateTimer_ = 0.0f;
                return true;
            }
        }
        return false;
                                                                 }));
    continuousStrafSeq->AddChild(std::make_unique<ActionNode>([this](Blackboard& bb) {
        auto owner = bb.Get<GameObject*>("Owner");
        ContinuousStrafAction(owner);
        return NodeStatus::Running; // 継続実行
                                                              }));
    combatSelector->AddChild(std::move(continuousStrafSeq));

    // 4b. 通常射撃（ストレイフしていない時）
    combatSelector->AddChild(std::make_unique<ActionNode>([this](Blackboard& bb) {
        auto owner = bb.Get<GameObject*>("Owner");
        isStrafing_ = false; // ストレイフ状態をリセット
        FireWeapon(owner);
        AimAtTarget(owner);
        return NodeStatus::Success;
                                                          }));

    combatSeq->AddChild(std::move(combatSelector));
    root->AddChild(std::move(combatSeq));

    // 5. パトロール：ターゲットが見えていなければ
    auto patrolSeq = std::make_unique<SequenceNode>();
    patrolSeq->AddChild(std::make_unique<ConditionNode>([this](Blackboard& bb) {
        return !bb.Get<bool>("IsTargetVisible");
                                                        }));
    patrolSeq->AddChild(std::make_unique<ActionNode>([this](Blackboard& bb) {
        auto owner = bb.Get<GameObject*>("Owner");
        PatrolAction(owner);
        return NodeStatus::Success;
                                                     }));
    root->AddChild(std::move(patrolSeq));

    // 6. Idle
    root->AddChild(std::make_unique<ActionNode>([this](Blackboard& bb) {
        auto owner = bb.Get<GameObject*>("Owner");
        IdleAction(owner);
        return NodeStatus::Running;
                                                }));

    behaviorTree_ = std::make_unique<BehaviorTree>(std::move(root));
}

// --- 各種アクションの中身例 ---
// ここでは省略せずに記述します。必要に応じて既存のメソッドから抜粋/整理してください。

void AssaultEnemyBehavior::IdleAction(GameObject* owner)
{
    // 何もしない、または待機アニメ再生など
}

void AssaultEnemyBehavior::PatrolAction(GameObject* owner)
{
    if (patrolPoints_.empty())
    {
        InitializePatrolPoints(owner->GetPosition(), patrolRadius_);
    }
    Vector3 targetPoint = patrolPoints_[currentPatrolIndex_];
    Vector3 dir = targetPoint - owner->GetPosition();
    float dist = dir.Length();
    if (dist < 1.5f)
    {
        currentPatrolIndex_ = (currentPatrolIndex_ + 1) % patrolPoints_.size();
        return;
    }
    dir.NormalizeSelf();
    float moveDistance = LimitMovementSpeed(moveSpeed_ * patrolSpeed_, TimeManager::GetInstance().GetDeltaTime());
    owner->SetPosition(owner->GetPosition() + dir * moveDistance);
    float angle = atan2(dir.x, dir.z);
    owner->SetRotation(Vector3(0, angle, 0));
}

void AssaultEnemyBehavior::RepositionAction(GameObject* owner)
{
    if (!target_) return;
    Vector3 targetPos = target_->GetPosition();
    Vector3 dir = targetPos - owner->GetPosition();
    float dist = dir.Length();
    float optimalDistance = (attackRange_ + minRange_) / 2.0f;
    repositionSpeed_ = std::min(repositionSpeed_ + 0.05f, maxRepositionSpeed_);
    dir.NormalizeSelf();
    float moveDistance = LimitMovementSpeed(moveSpeed_, TimeManager::GetInstance().GetDeltaTime());
    if (dist > optimalDistance)
    {
        owner->SetPosition(owner->GetPosition() + dir * moveDistance * repositionSpeed_);
    }
    else
    {
        owner->SetPosition(owner->GetPosition() - dir * moveDistance * repositionSpeed_);
    }
}

void AssaultEnemyBehavior::StrafeAction(GameObject* owner)
{
    if (!target_) return;
    strafeTimer_ += TimeManager::GetInstance().GetDeltaTime();
    if (strafeTimer_ > strafeChangeInterval_)
    {
        strafeDirection_ = GetRandomStrafeDirection(owner);
        strafeTimer_ = 0.0f;
    }
    float moveDistance = LimitMovementSpeed(moveSpeed_ * 0.6f, TimeManager::GetInstance().GetDeltaTime());
    owner->SetPosition(owner->GetPosition() + strafeDirection_ * moveDistance);
    if (IsInAttackRange(owner))
    {
        FireWeapon(owner);
    }
}

void AssaultEnemyBehavior::RetreatAction(GameObject* owner)
{
    if (!target_) return;
    Vector3 targetPos = target_->GetPosition();
    Vector3 dir = targetPos - owner->GetPosition();
    dir.NormalizeSelf();
    Vector3 retreatDir = -dir;
    float moveDistance = LimitMovementSpeed(moveSpeed_ * 1.2f, TimeManager::GetInstance().GetDeltaTime());
    owner->SetPosition(owner->GetPosition() + retreatDir * moveDistance);
}

// --- 既存の補助メソッド（省略せずに全て記述してください） ---
// AimAtTarget, FireWeapon, IsTargetVisible, IsInAttackRange, IsInExtendedAttackRange, GetRandomStrafeDirection, InitializePatrolPoints,
// CalculateSmoothMovement, LimitMovementSpeed, ForceMovement, IsStuck, などは元のコードから移植してください。

void AssaultEnemyBehavior::AimAtTarget(GameObject* owner)
{
    if (!target_) return;
    Vector3 targetPos = target_->GetPosition();
    Vector3 direction = targetPos - owner->GetPosition();
    direction.NormalizeSelf();
    float angle = atan2(direction.x, direction.z);
    owner->SetRotation(Vector3(0, angle, 0));
}

void AssaultEnemyBehavior::FireWeapon(GameObject* owner)
{
    // アサルトライフルコンポーネントのFire()メソッドを呼び出す
    if (auto weapon = owner->GetComponent<AssaultRifleComponent>())
    {
        weapon->Fire();
    }
}

bool AssaultEnemyBehavior::IsTargetVisible(GameObject* owner)
{
    if (!target_) return false;
    Vector3 targetPos = target_->GetPosition();
    Vector3 direction = targetPos - owner->GetPosition();
    float distance = direction.Length();
    return (distance <= detectionRange_);
}

bool AssaultEnemyBehavior::IsInAttackRange(GameObject* owner)
{
    if (!target_) return false;
    Vector3 targetPos = target_->GetPosition();
    Vector3 direction = targetPos - owner->GetPosition();
    float distance = direction.Length();
    return (distance >= minRange_ && distance <= maxRange_);
}

bool AssaultEnemyBehavior::IsInExtendedAttackRange(GameObject* owner)
{
    if (!target_) return false;
    Vector3 targetPos = target_->GetPosition();
    Vector3 direction = targetPos - owner->GetPosition();
    float distance = direction.Length();
    return (distance >= extendedMinRange_ && distance <= extendedMaxRange_);
}

Vector3 AssaultEnemyBehavior::GetRandomStrafeDirection(GameObject* owner)
{
    if (!target_) return Vector3(1.0f, 0, 0);

    Vector3 toTarget = target_->GetPosition() - owner->GetPosition();
    float distanceToTarget = toTarget.Length();
    toTarget.NormalizeSelf();

    // 横方向ベクトル
    Vector3 right(toTarget.z, 0, -toTarget.x);

    // ランダム要素
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    float randomValue = dist(rng_);

    // ターゲットとの距離に応じて円運動を調整
    float optimalDistance = (attackRange_ + minRange_) / 2.0f;
    float distanceFactor = std::min(1.0f, std::abs(distanceToTarget - optimalDistance) / (maxRange_ - minRange_));

    // 横方向と前後方向を混ぜる
    Vector3 strafeDir;
    if (distanceToTarget > optimalDistance)
    {
        // 遠い場合は接近しながら横移動
        strafeDir = right * randomValue * strafeTendencyFactor_ + toTarget * (1.0f - strafeTendencyFactor_ + distanceFactor * 0.3f);
    }
    else
    {
        // 近い場合は離れながら横移動
        strafeDir = right * randomValue * strafeTendencyFactor_ - toTarget * (1.0f - strafeTendencyFactor_ + distanceFactor * 0.3f);
    }

    strafeDir.NormalizeSelf();
    return strafeDir;
}

void AssaultEnemyBehavior::InitializePatrolPoints(const Vector3& centerPoint, float radius)
{
    patrolPoints_.clear();

    // 8点の巡回ポイントを生成
    const int numPoints = 8;
    for (int i = 0; i < numPoints; i++)
    {
        float angle = (i * 2.0f * 3.14159f) / numPoints;
        float x = centerPoint.x + radius * std::cos(angle);
        float z = centerPoint.z + radius * std::sin(angle);
        patrolPoints_.push_back(Vector3(x, centerPoint.y, z));
    }

    // ランダムな開始位置
    std::random_device rd;
    rng_ = std::mt19937(rd());
    currentPatrolIndex_ = std::uniform_int_distribution<int>(0, numPoints - 1)(rng_);
    patrolInitialized_ = true;
}

Vector3 AssaultEnemyBehavior::CalculateSmoothMovement(const Vector3& currentPos, const Vector3& targetPos, float maxDistance)
{
    Vector3 direction = targetPos - currentPos;
    float distance = direction.Length();

    if (distance <= maxDistance)
    {
        return targetPos;
    }

    direction.NormalizeSelf();
    return currentPos + direction * maxDistance;
}

float AssaultEnemyBehavior::LimitMovementSpeed(float baseSpeed, float dt)
{
    // 1フレームあたりの移動距離に上限を設定
    return std::min(baseSpeed * dt, maxMoveDistancePerFrame_);
}

bool AssaultEnemyBehavior::IsStuck(GameObject* owner)
{
    Vector3 currentPos = owner->GetPosition();
    float movement = (currentPos - lastPosition_).Length();

    if (movement < 0.01f)
    {
        stuckTimer_ += TimeManager::GetInstance().GetDeltaTime();

        if (stuckTimer_ > stuckThreshold_)
        {
            stuckTimer_ = 0.0f;
            return true;
        }
    }
    else
    {
        stuckTimer_ = 0.0f;
    }

    return false;
}

void AssaultEnemyBehavior::ForceMovement(GameObject* owner)
{
    // スタック状態を解消するための緊急移動

    // 現在の状態を変更
    // BT化でcurrentState_未使用の場合は不要だが、もし使いたければここで分岐も可能

    // 強制的に少し動かす
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    Vector3 randomDir(dist(rng_), 0, dist(rng_));
    randomDir.NormalizeSelf();

    float forceMove = moveSpeed_ * 0.5f * (TimeManager::GetInstance().GetDeltaTime());
    owner->SetPosition(owner->GetPosition() + randomDir * forceMove);
}