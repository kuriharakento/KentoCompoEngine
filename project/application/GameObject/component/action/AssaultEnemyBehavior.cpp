#include "AssaultEnemyBehavior.h"
#include "AssaultRifleComponent.h"
#include "application/GameObject/base/GameObject.h"
#include "math/MathUtils.h"
#include <cmath>
#include <algorithm>
#include <random>

#include "time/TimeManager.h"

AssaultEnemyBehavior::AssaultEnemyBehavior(GameObject* target) : target_(target)
{
    // 乱数生成器の初期化
    std::random_device rd;
    rng_ = std::mt19937(rd());

    // ターゲットがある場合は戦闘状態からスタート
    if (target_)
    {
        currentState_ = State::Combat;
    }

    // 初期位置を有効な位置として保存
    lastValidPosition_ = Vector3(0, 0, 0);
    lastPosition_ = Vector3(0, 0, 0);
}

void AssaultEnemyBehavior::Update(GameObject* owner)
{
    // タイマー更新
	float deltaTime = TimeManager::GetInstance().GetDeltaTime();
    stateTimer_ += deltaTime;
    positionCheckTimer_ += deltaTime;

    if (actionCooldown_ > 0)
    {
        actionCooldown_ -= deltaTime;
    }

    // 動き停止の検出
    if (IsStuck(owner))
    {
        ForceMovement(owner);
    }

    // 最後の位置を更新
    lastPosition_ = owner->GetPosition();

    // 一定間隔で現在位置を有効な位置として保存
    if (positionCheckTimer_ > 2.0f && IsInExtendedAttackRange(owner))
    {
        lastValidPosition_ = owner->GetPosition();
        positionCheckTimer_ = 0.0f;
    }

    // ターゲットがなくなった場合はIdle状態へ
    if (!target_ && currentState_ != State::Idle)
    {
        currentState_ = State::Idle;
        stateTimer_ = 0.0f;
    }

    // ターゲットがあるが、検知範囲外の場合は巡回状態へ
    if (target_ && !IsTargetVisible(owner) &&
        currentState_ != State::Patrol && currentState_ != State::Idle)
    {
        currentState_ = State::Patrol;

        // 初回のみパトロールポイントを初期化
        if (!patrolInitialized_)
        {
            InitializePatrolPoints(owner->GetPosition(), patrolRadius_);
            patrolInitialized_ = true;
        }
        stateTimer_ = 0.0f;
    }

    // ステートマシン
    switch (currentState_)
    {
    case State::Idle:
        IdleBehavior(owner);
        break;
    case State::Combat:
        CombatBehavior(owner);
        break;
    case State::Patrol:
        PatrolBehavior(owner);
        break;
    case State::Reposition:
        RepositionBehavior(owner);
        break;
    case State::Strafe:
        StrafeBehavior(owner);
        break;
    case State::Retreat:
        RetreatBehavior(owner);
        break;
    }

    // 戦闘中かつターゲットが見えている場合は常にターゲットの方を向く
    if (target_ && (currentState_ == State::Combat ||
                    currentState_ == State::Reposition ||
                    currentState_ == State::Strafe ||
                    currentState_ == State::Retreat))
    {
        AimAtTarget(owner);
    }
}

void AssaultEnemyBehavior::IdleBehavior(GameObject* owner)
{
    // 待機状態 - ターゲットを見つけたら戦闘状態へ
    if (target_ && IsTargetVisible(owner))
    {
        currentState_ = State::Combat;
        stateTimer_ = 0.0f;
    }
}

void AssaultEnemyBehavior::CombatBehavior(GameObject* owner)
{
    if (!target_)
    {
        currentState_ = State::Idle;
        return;
    }

    // ターゲットとの距離を計算
    Vector3 targetPos = target_->GetPosition();
    Vector3 direction = targetPos - owner->GetPosition();
    float distance = direction.Length();

    // ターゲットが検知範囲外なら巡回へ
    if (distance > detectionRange_)
    {
        currentState_ = State::Patrol;
        if (!patrolInitialized_)
        {
            InitializePatrolPoints(owner->GetPosition(), patrolRadius_);
            patrolInitialized_ = true;
        }
        stateTimer_ = 0.0f;
        return;
    }

    // 距離による行動分岐
    if (distance < extendedMinRange_)
    {
        // 近すぎる場合は後退
        currentState_ = State::Retreat;
        stateTimer_ = 0.0f;
    }
    else if (distance > extendedMaxRange_)
    {
        // 遠すぎる場合は接近
        currentState_ = State::Reposition;
        stateTimer_ = 0.0f;
        repositionSpeed_ = 0.0f;
    }
    else if (IsInAttackRange(owner))
    {
        // 攻撃範囲内なら攻撃
        FireWeapon(owner);

        // 一定時間経過後に横移動へ
        if (stateTimer_ > 1.5f + (std::uniform_real_distribution<float>(0, 1)(rng_) * 1.0f))
        {
            currentState_ = State::Strafe;
            strafeDirection_ = GetRandomStrafeDirection(owner);
            stateTimer_ = 0.0f;
            strafeTimer_ = 0.0f;
        }
    }
    else if (!IsInExtendedAttackRange(owner))
    {
        // 拡張範囲外なら位置調整
        currentState_ = State::Reposition;
        stateTimer_ = 0.0f;
        repositionSpeed_ = 0.0f;
    }
    else
    {
        // 拡張範囲内だが攻撃範囲外の場合、確率で横移動か位置調整へ
        if (stateTimer_ > 1.5f)
        {
            if (std::uniform_real_distribution<float>(0, 1)(rng_) < 0.7f)
            {
                currentState_ = State::Strafe;
                strafeDirection_ = GetRandomStrafeDirection(owner);
            }
            else
            {
                currentState_ = State::Reposition;
                repositionSpeed_ = 0.0f;
            }
            stateTimer_ = 0.0f;
            strafeTimer_ = 0.0f;
        }
    }
}

void AssaultEnemyBehavior::PatrolBehavior(GameObject* owner)
{
    // パトロールポイントがなければ初期化
    if (patrolPoints_.empty())
    {
        InitializePatrolPoints(owner->GetPosition(), patrolRadius_);
    }

    // ターゲットが検知範囲内に入ったら戦闘状態へ
    if (target_ && IsTargetVisible(owner))
    {
        currentState_ = State::Combat;
        stateTimer_ = 0.0f;
        return;
    }

    // 現在のパトロールポイントへ移動
    Vector3 targetPoint = patrolPoints_[currentPatrolIndex_];
    Vector3 direction = targetPoint - owner->GetPosition();
    float distance = direction.Length();

    // パトロールポイントに到着したら次のポイントへ
    if (distance < 1.5f)
    {
        currentPatrolIndex_ = (currentPatrolIndex_ + 1) % patrolPoints_.size();
        return;
    }

    // 移動
    direction.NormalizeSelf();
    float moveDistance = LimitMovementSpeed(moveSpeed_ * patrolSpeed_, TimeManager::GetInstance().GetDeltaTime());
    owner->SetPosition(owner->GetPosition() + direction * moveDistance);

    // 移動方向を向く
    float angle = atan2(direction.x, direction.z);
    owner->SetRotation(Vector3(0, angle, 0));

    // 一定時間ごとにターゲットの位置を確認
    if (stateTimer_ > 3.0f)
    {
        stateTimer_ = 0.0f;

        // ターゲットが存在し、検知範囲内なら戦闘状態へ
        if (target_ && IsTargetVisible(owner))
        {
            currentState_ = State::Combat;
        }
    }
}

void AssaultEnemyBehavior::RepositionBehavior(GameObject* owner)
{
    if (!target_)
    {
        currentState_ = State::Idle;
        return;
    }

    // ターゲット方向を計算
    Vector3 targetPos = target_->GetPosition();
    Vector3 direction = targetPos - owner->GetPosition();
    float distance = direction.Length();

    // 検知範囲外なら巡回へ
    if (distance > detectionRange_)
    {
        currentState_ = State::Patrol;
        stateTimer_ = 0.0f;
        return;
    }

    // 最適な攻撃距離を目指す
    float optimalDistance = (attackRange_ + minRange_) / 2.0f;

    // 徐々に加速（滑らかな動きのため）
    repositionSpeed_ = std::min(repositionSpeed_ + 0.05f, maxRepositionSpeed_);

    // 遠すぎる場合は位置復帰
    if (distance > extendedMaxRange_ * 1.5f)
    {
        // 範囲が大きすぎる場合は最後の有効位置に戻る
        if (lastValidPosition_ != Vector3(0, 0, 0))
        {
            Vector3 toLastValid = lastValidPosition_ - owner->GetPosition();
            float lastValidDist = toLastValid.Length();

            if (lastValidDist > 0.5f)
            {
                toLastValid.NormalizeSelf();
                float moveDistance = LimitMovementSpeed(moveSpeed_ * 1.2f, TimeManager::GetInstance().GetDeltaTime());
                owner->SetPosition(owner->GetPosition() + toLastValid * moveDistance * repositionSpeed_);
                return;
            }
        }
    }

    // 最適距離に到達したら戦闘状態へ
    if (std::abs(distance - optimalDistance) < 2.0f)
    {
        currentState_ = State::Combat;
        stateTimer_ = 0.0f;
        return;
    }

    // ターゲットへの移動ベクトル
    direction.NormalizeSelf();
    float moveDistance = LimitMovementSpeed(moveSpeed_, TimeManager::GetInstance().GetDeltaTime());

    // 最適距離より遠い場合は接近、近い場合は後退
    if (distance > optimalDistance)
    {
        owner->SetPosition(owner->GetPosition() + direction * moveDistance * repositionSpeed_);
    }
    else
    {
        owner->SetPosition(owner->GetPosition() - direction * moveDistance * repositionSpeed_);
    }

    // 一定時間経過でストレイフへ切り替え
    if (stateTimer_ > 2.0f)
    {
        currentState_ = State::Strafe;
        strafeDirection_ = GetRandomStrafeDirection(owner);
        stateTimer_ = 0.0f;
        strafeTimer_ = 0.0f;
    }
}

void AssaultEnemyBehavior::StrafeBehavior(GameObject* owner)
{
    if (!target_)
    {
        currentState_ = State::Idle;
        return;
    }

    strafeTimer_ += TimeManager::GetInstance().GetDeltaTime();

    // ターゲットとの距離を計算
    Vector3 targetPos = target_->GetPosition();
    Vector3 direction = targetPos - owner->GetPosition();
    float distance = direction.Length();

    // 検知範囲外なら巡回へ
    if (distance > detectionRange_)
    {
        currentState_ = State::Patrol;
        stateTimer_ = 0.0f;
        return;
    }

    // 一定時間ごとに横移動方向を変更
    if (strafeTimer_ > strafeChangeInterval_)
    {
        strafeDirection_ = GetRandomStrafeDirection(owner);
        strafeTimer_ = 0.0f;
    }

    // 横移動を適用
    float moveDistance = LimitMovementSpeed(moveSpeed_ * 0.6f, TimeManager::GetInstance().GetDeltaTime());
    owner->SetPosition(owner->GetPosition() + strafeDirection_ * moveDistance);

    // 攻撃可能なら攻撃
    if (IsInAttackRange(owner))
    {
        FireWeapon(owner);
    }

    // 一定時間経過で戦闘状態へ戻る
    if (stateTimer_ > 2.0f)
    {
        currentState_ = State::Combat;
        stateTimer_ = 0.0f;
    }

    // 範囲外に大きく外れたら位置調整
    if (distance < extendedMinRange_ * 0.7f || distance > extendedMaxRange_ * 1.3f)
    {
        currentState_ = State::Reposition;
        stateTimer_ = 0.0f;
        repositionSpeed_ = 0.0f;
    }
}

void AssaultEnemyBehavior::RetreatBehavior(GameObject* owner)
{
    if (!target_)
    {
        currentState_ = State::Idle;
        return;
    }

    // ターゲットとの距離を計算
    Vector3 targetPos = target_->GetPosition();
    Vector3 direction = targetPos - owner->GetPosition();
    float distance = direction.Length();

    // 検知範囲外なら巡回へ
    if (distance > detectionRange_)
    {
        currentState_ = State::Patrol;
        stateTimer_ = 0.0f;
        return;
    }

    // ターゲットから離れる方向を計算
    direction.NormalizeSelf();
    Vector3 retreatDirection = -direction;

    // 後退移動
    float moveDistance = LimitMovementSpeed(moveSpeed_ * 1.2f, TimeManager::GetInstance().GetDeltaTime());
    owner->SetPosition(owner->GetPosition() + retreatDirection * moveDistance);

    // 十分な距離が確保できたら戦闘状態へ
    if (distance >= minRange_ * 1.2f)
    {
        currentState_ = State::Combat;
        stateTimer_ = 0.0f;
    }

    // 一定時間経過でも横移動へ
    if (stateTimer_ > 1.0f)
    {
        currentState_ = State::Strafe;
        strafeDirection_ = GetRandomStrafeDirection(owner);
        stateTimer_ = 0.0f;
        strafeTimer_ = 0.0f;
    }
}

void AssaultEnemyBehavior::AimAtTarget(GameObject* owner)
{
    // ターゲット方向を向く
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

    // 距離チェック
    return (distance <= detectionRange_);
}

bool AssaultEnemyBehavior::IsInAttackRange(GameObject* owner)
{
    if (!target_) return false;

    Vector3 targetPos = target_->GetPosition();
    Vector3 direction = targetPos - owner->GetPosition();
    float distance = direction.Length();

    // 最適攻撃距離の範囲内にいるか
    return (distance >= minRange_ && distance <= maxRange_);
}

bool AssaultEnemyBehavior::IsInExtendedAttackRange(GameObject* owner)
{
    if (!target_) return false;

    Vector3 targetPos = target_->GetPosition();
    Vector3 direction = targetPos - owner->GetPosition();
    float distance = direction.Length();

    // 拡張された攻撃範囲
    return (distance >= extendedMinRange_ && distance <= extendedMaxRange_);
}

Vector3 AssaultEnemyBehavior::GetRandomStrafeDirection(GameObject* owner)
{
    if (!target_) return Vector3(1.0f, 0, 0);

    // ターゲットへの方向
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
    currentPatrolIndex_ = std::uniform_int_distribution<int>(0, numPoints - 1)(rng_);
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
    // 前回の位置と現在の位置を比較
    Vector3 currentPos = owner->GetPosition();
    float movement = (currentPos - lastPosition_).Length();

    // 動きがほぼない場合
    if (movement < 0.01f)
    {
        stuckTimer_ += TimeManager::GetInstance().GetDeltaTime();

        // 一定時間動いていなければスタック状態と判定
        if (stuckTimer_ > stuckThreshold_)
        {
            stuckTimer_ = 0.0f;
            return true;
        }
    }
    else
    {
        // 動いていればリセット
        stuckTimer_ = 0.0f;
    }

    return false;
}

void AssaultEnemyBehavior::ForceMovement(GameObject* owner)
{
    // スタック状態を解消するための緊急移動

    // 現在の状態を変更
    if (currentState_ == State::Combat || currentState_ == State::Reposition)
    {
        // 横移動に変更
        currentState_ = State::Strafe;
        strafeDirection_ = GetRandomStrafeDirection(owner);
        stateTimer_ = 0.0f;
        strafeTimer_ = 0.0f;
    }
    else if (currentState_ == State::Strafe)
    {
        // 方向を変える
        strafeDirection_ = GetRandomStrafeDirection(owner);
        strafeTimer_ = 0.0f;
    }

    // 強制的に少し動かす
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    Vector3 randomDir(dist(rng_), 0, dist(rng_));
    randomDir.NormalizeSelf();

    // ランダムな方向に少し移動
    float forceMove = moveSpeed_ * 0.5f * (TimeManager::GetInstance().GetDeltaTime());
    owner->SetPosition(owner->GetPosition() + randomDir * forceMove);
}