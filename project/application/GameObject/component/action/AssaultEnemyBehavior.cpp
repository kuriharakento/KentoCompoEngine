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

// コンストラクタ：乱数生成器の初期化とビヘイビアツリーの構築
AssaultEnemyBehavior::AssaultEnemyBehavior(GameObject* target) : target_(target)
{
	// 乱数生成器を初期化
	std::random_device rd;
	rng_ = std::mt19937(rd());
	// ビヘイビアツリーを構築
	BuildBehaviorTree();
}

// フレームごとの更新処理
void AssaultEnemyBehavior::Update(GameObject* owner)
{
	// デルタタイムを取得
	float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;

	// 各種タイマーを更新
	stateTimer_ += deltaTime;
	strafeTimer_ += deltaTime;
	positionCheckTimer_ += deltaTime;
	combatStateTimer_ += deltaTime;

	// 行動クールダウンを減少
	if (actionCooldown_ > 0) actionCooldown_ -= deltaTime;

	// 前フレームの位置を保存（スタック検出用）
	lastPosition_ = owner->GetPosition();

	// Blackboardへ状態情報をセット
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

	// ビヘイビアツリーを実行
	behaviorTree_->Tick();
}

// 継続的なストレイフ行動
void AssaultEnemyBehavior::ContinuousStrafAction(GameObject* owner)
{
	// ターゲットが存在しない場合はストレイフを終了
	if (!target_)
	{
		isStrafing_ = false;
		return;
	}

	float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;
	strafeTimer_ += deltaTime;

	// ストレイフ継続時間チェック
	if (strafeTimer_ >= strafeDuration_)
	{
		isStrafing_ = false;
		strafeTimer_ = 0.0f;
		return;
	}

	// ストレイフ方向を定期的に変更
	if (fmod(strafeTimer_, strafeChangeInterval_) < deltaTime)
	{
		strafeDirection_ = GetRandomStrafeDirection(owner);
	}

	// ストレイフ移動を実行
	float strafeSpeed = moveSpeed_ * kStrafeSpeedMultiplier;
	float moveDistance = LimitMovementSpeed(strafeSpeed, deltaTime);
	Vector3 newPosition = owner->GetPosition() + strafeDirection_ * moveDistance;
	owner->SetPosition(newPosition);

	// ストレイフ中も継続的に射撃
	if (actionCooldown_ <= 0.0f)
	{
		FireWeapon(owner);
		actionCooldown_ = kShootIntervalDuringStrafe;
	}

	// エイミングも継続
	AimAtTarget(owner);
}

// ビヘイビアツリーの構築
void AssaultEnemyBehavior::BuildBehaviorTree()
{
	auto root = std::make_unique<SelectorNode>();

	// 1. スタック検知で強制移動
	auto stuckSeq = std::make_unique<SequenceNode>();
	stuckSeq->AddChild(std::make_unique<ConditionNode>([this](Blackboard& bb) {
		auto owner = bb.Get<GameObject*>("Owner");
		return IsStuck(owner);
													   }));
	stuckSeq->AddChild(std::make_unique<ActionNode>(
		"Move",
		[this](Blackboard& bb) {
			auto owner = bb.Get<GameObject*>("Owner");
			ForceMovement(owner);
			return NodeStatus::Success;
		}));
	root->AddChild(std::move(stuckSeq));

	// 2. 距離が近すぎる場合は後退
	auto retreatSeq = std::make_unique<SequenceNode>();
	retreatSeq->AddChild(std::make_unique<ConditionNode>([this](Blackboard& bb) {
		auto owner = bb.Get<GameObject*>("Owner");
		auto target = bb.Get<GameObject*>("Target");
		if (!target) return false;
		float dist = (target->GetPosition() - owner->GetPosition()).Length();
		return dist < minRange_;
														 }));
	retreatSeq->AddChild(std::make_unique<ActionNode>(
		"Back",
		[this](Blackboard& bb) {
			auto owner = bb.Get<GameObject*>("Owner");
			RetreatAction(owner);
			return NodeStatus::Success;
		}));
	root->AddChild(std::move(retreatSeq));

	// 3. 距離が遠すぎる場合はリポジション
	auto repositionSeq = std::make_unique<SequenceNode>();
	repositionSeq->AddChild(std::make_unique<ConditionNode>([this](Blackboard& bb) {
		auto owner = bb.Get<GameObject*>("Owner");
		auto target = bb.Get<GameObject*>("Target");
		if (!target) return false;
		float dist = (target->GetPosition() - owner->GetPosition()).Length();
		return dist > maxRange_;
															}));
	repositionSeq->AddChild(std::make_unique<ActionNode>(
		"RePosition",
		[this](Blackboard& bb) {
			auto owner = bb.Get<GameObject*>("Owner");
			RepositionAction(owner);
			return NodeStatus::Success;
		}));
	root->AddChild(std::move(repositionSeq));

	// 4. 戦闘：ターゲットが見えて攻撃範囲内の場合
	auto combatSeq = std::make_unique<SequenceNode>();
	combatSeq->AddChild(std::make_unique<ConditionNode>([this](Blackboard& bb) {
		return bb.Get<bool>("IsTargetVisible") && bb.Get<bool>("IsInAttackRange");
														}));

	// 戦闘時のセレクター（継続的なストレイフまたは射撃）
	auto combatSelector = std::make_unique<SelectorNode>();

	// 4a. 継続的ストレイフ（一定期間継続）
	auto continuousStrafSeq = std::make_unique<SequenceNode>();
	continuousStrafSeq->AddChild(std::make_unique<ConditionNode>([this](Blackboard& bb) {
		// ストレイフ状態が継続中の場合
		if (isStrafing_)
		{
			return strafeTimer_ < strafeDuration_;
		}
		else
		{
			// 新たにストレイフを開始する条件をチェック
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
	continuousStrafSeq->AddChild(std::make_unique<ActionNode>(
		"Strafe",
		[this](Blackboard& bb) {
			auto owner = bb.Get<GameObject*>("Owner");
			ContinuousStrafAction(owner);
			return NodeStatus::Running;
		}));
	combatSelector->AddChild(std::move(continuousStrafSeq));

	// 4b. 通常射撃（ストレイフしていない時）
	combatSelector->AddChild(std::make_unique<ActionNode>(
		"Fire",
		[this](Blackboard& bb) {
			auto owner = bb.Get<GameObject*>("Owner");
			isStrafing_ = false;
			FireWeapon(owner);
			AimAtTarget(owner);
			return NodeStatus::Success;
		}));

	combatSeq->AddChild(std::move(combatSelector));
	root->AddChild(std::move(combatSeq));

	// 5. パトロール：ターゲットが見えていない場合
	auto patrolSeq = std::make_unique<SequenceNode>();
	patrolSeq->AddChild(std::make_unique<ConditionNode>([this](Blackboard& bb) {
		return !bb.Get<bool>("IsTargetVisible");
														}));
	patrolSeq->AddChild(std::make_unique<ActionNode>(
		"Patrol",
		[this](Blackboard& bb) {
			auto owner = bb.Get<GameObject*>("Owner");
			PatrolAction(owner);
			return NodeStatus::Success;
		}));
	root->AddChild(std::move(patrolSeq));

	// 6. 待機行動（他の条件に該当しない場合）
	root->AddChild(std::make_unique<ActionNode>(
		"Idle",
		[this](Blackboard& bb) {
			auto owner = bb.Get<GameObject*>("Owner");
			IdleAction(owner);
			return NodeStatus::Running;
		}));

	behaviorTree_ = std::make_unique<BehaviorTree>(std::move(root));
}

// 待機行動（何もしない）
void AssaultEnemyBehavior::IdleAction(GameObject* owner)
{
	// 何もしない、または待機アニメ再生など
}

// パトロール行動
void AssaultEnemyBehavior::PatrolAction(GameObject* owner)
{
	// パトロールポイントが未初期化の場合は初期化
	if (patrolPoints_.empty())
	{
		InitializePatrolPoints(owner->GetPosition(), patrolRadius_);
	}

	// 目標のパトロールポイントを取得
	Vector3 targetPoint = patrolPoints_[currentPatrolIndex_];
	Vector3 dir = targetPoint - owner->GetPosition();
	float dist = dir.Length();

	// 目標地点に到達した場合、次のポイントへ
	if (dist < kPatrolArrivalThreshold)
	{
		currentPatrolIndex_ = (currentPatrolIndex_ + 1) % patrolPoints_.size();
		return;
	}

	// 目標地点へ移動
	dir.NormalizeSelf();
	float moveDistance = LimitMovementSpeed(moveSpeed_ * patrolSpeed_, TimeManager::GetInstance().GetGameContext().deltaTime);
	owner->SetPosition(owner->GetPosition() + dir * moveDistance);

	// 移動方向を向く
	float angle = atan2(dir.x, dir.z);
	owner->SetRotation(Vector3(0, angle, 0));
}

// 位置調整行動
void AssaultEnemyBehavior::RepositionAction(GameObject* owner)
{
	if (!target_) return;

	Vector3 targetPos = target_->GetPosition();
	Vector3 dir = targetPos - owner->GetPosition();
	float dist = dir.Length();

	// 最適距離を計算
	float optimalDistance = (attackRange_ + minRange_) / 2.0f;

	// 位置調整速度を徐々に上げる
	repositionSpeed_ = std::min(repositionSpeed_ + kRepositionAcceleration, maxRepositionSpeed_);
	dir.NormalizeSelf();
	float moveDistance = LimitMovementSpeed(moveSpeed_, TimeManager::GetInstance().GetGameContext().deltaTime);

	// 最適距離より遠い場合は接近、近い場合は離れる
	if (dist > optimalDistance)
	{
		owner->SetPosition(owner->GetPosition() + dir * moveDistance * repositionSpeed_);
	}
	else
	{
		owner->SetPosition(owner->GetPosition() - dir * moveDistance * repositionSpeed_);
	}
}

// 横移動行動
void AssaultEnemyBehavior::StrafeAction(GameObject* owner)
{
	if (!target_) return;

	strafeTimer_ += TimeManager::GetInstance().GetGameContext().deltaTime;

	// 一定時間ごとにストレイフ方向を変更
	if (strafeTimer_ > strafeChangeInterval_)
	{
		strafeDirection_ = GetRandomStrafeDirection(owner);
		strafeTimer_ = 0.0f;
	}

	// ストレイフ移動を実行
	float moveDistance = LimitMovementSpeed(moveSpeed_ * kStrafeActionSpeedMultiplier, TimeManager::GetInstance().GetGameContext().deltaTime);
	owner->SetPosition(owner->GetPosition() + strafeDirection_ * moveDistance);

	// 攻撃範囲内なら射撃
	if (IsInAttackRange(owner))
	{
		FireWeapon(owner);
	}
}

// 後退行動
void AssaultEnemyBehavior::RetreatAction(GameObject* owner)
{
	if (!target_) return;

	// ターゲットから離れる方向を計算
	Vector3 targetPos = target_->GetPosition();
	Vector3 dir = targetPos - owner->GetPosition();
	dir.NormalizeSelf();
	Vector3 retreatDir = -dir;

	// 後退移動を実行
	float moveDistance = LimitMovementSpeed(moveSpeed_ * kRetreatSpeedMultiplier, TimeManager::GetInstance().GetGameContext().deltaTime);
	owner->SetPosition(owner->GetPosition() + retreatDir * moveDistance);
}

// ターゲットに照準を合わせる
void AssaultEnemyBehavior::AimAtTarget(GameObject* owner)
{
	if (!target_) return;

	// ターゲット方向を計算して向きを設定
	Vector3 targetPos = target_->GetPosition();
	Vector3 direction = targetPos - owner->GetPosition();
	direction.NormalizeSelf();
	float angle = atan2(direction.x, direction.z);
	owner->SetRotation(Vector3(0, angle, 0));
}

// 武器を発射する
void AssaultEnemyBehavior::FireWeapon(GameObject* owner)
{
	// アサルトライフルコンポーネントのFire()メソッドを呼び出す
	if (auto weapon = owner->GetComponent<AssaultRifleComponent>())
	{
		weapon->Fire();
	}
}

// ターゲットが視界内にいるか確認
bool AssaultEnemyBehavior::IsTargetVisible(GameObject* owner)
{
	if (!target_) return false;

	// ターゲットまでの距離を計算
	Vector3 targetPos = target_->GetPosition();
	Vector3 direction = targetPos - owner->GetPosition();
	float distance = direction.Length();

	// 検知範囲内にいるかどうかを返す
	return (distance <= detectionRange_);
}

// 攻撃範囲内にいるか確認
bool AssaultEnemyBehavior::IsInAttackRange(GameObject* owner)
{
	if (!target_) return false;

	// ターゲットまでの距離を計算
	Vector3 targetPos = target_->GetPosition();
	Vector3 direction = targetPos - owner->GetPosition();
	float distance = direction.Length();

	// 最小〜最大範囲内にいるかどうかを返す
	return (distance >= minRange_ && distance <= maxRange_);
}

// 拡張攻撃範囲内にいるか確認
bool AssaultEnemyBehavior::IsInExtendedAttackRange(GameObject* owner)
{
	if (!target_) return false;

	// ターゲットまでの距離を計算
	Vector3 targetPos = target_->GetPosition();
	Vector3 direction = targetPos - owner->GetPosition();
	float distance = direction.Length();

	// 拡張最小〜拡張最大範囲内にいるかどうかを返す
	return (distance >= extendedMinRange_ && distance <= extendedMaxRange_);
}

// ランダムな横移動方向を取得
Vector3 AssaultEnemyBehavior::GetRandomStrafeDirection(GameObject* owner)
{
	if (!target_) return Vector3(1.0f, 0, 0);

	// ターゲットへの方向を計算
	Vector3 toTarget = target_->GetPosition() - owner->GetPosition();
	float distanceToTarget = toTarget.Length();
	toTarget.NormalizeSelf();

	// 横方向ベクトルを計算
	Vector3 right(toTarget.z, 0, -toTarget.x);

	// ランダム要素を生成
	std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
	float randomValue = dist(rng_);

	// ターゲットとの距離に応じて円運動を調整
	float optimalDistance = (attackRange_ + minRange_) / 2.0f;
	float distanceFactor = std::min(1.0f, std::abs(distanceToTarget - optimalDistance) / (maxRange_ - minRange_));

	// 横方向と前後方向を混ぜてストレイフ方向を決定
	Vector3 strafeDir;
	if (distanceToTarget > optimalDistance)
	{
		// 遠い場合は接近しながら横移動
		strafeDir = right * randomValue * strafeTendencyFactor_ + toTarget * (1.0f - strafeTendencyFactor_ + distanceFactor * kDistanceFactorAdjustment);
	}
	else
	{
		// 近い場合は離れながら横移動
		strafeDir = right * randomValue * strafeTendencyFactor_ - toTarget * (1.0f - strafeTendencyFactor_ + distanceFactor * kDistanceFactorAdjustment);
	}

	strafeDir.NormalizeSelf();
	return strafeDir;
}

// パトロールポイントを初期化
void AssaultEnemyBehavior::InitializePatrolPoints(const Vector3& centerPoint, float radius)
{
	patrolPoints_.clear();

	// 円周上に等間隔でパトロールポイントを生成
	for (int i = 0; i < kPatrolPointCount; i++)
	{
		float angle = (i * 2.0f * 3.14159f) / kPatrolPointCount;
		float x = centerPoint.x + radius * std::cos(angle);
		float z = centerPoint.z + radius * std::sin(angle);
		patrolPoints_.push_back(Vector3(x, centerPoint.y, z));
	}

	// ランダムな開始位置を設定
	std::random_device rd;
	rng_ = std::mt19937(rd());
	currentPatrolIndex_ = std::uniform_int_distribution<int>(0, kPatrolPointCount - 1)(rng_);
	patrolInitialized_ = true;
}

// スムーズな移動を計算
Vector3 AssaultEnemyBehavior::CalculateSmoothMovement(const Vector3& currentPos, const Vector3& targetPos, float maxDistance)
{
	Vector3 direction = targetPos - currentPos;
	float distance = direction.Length();

	// 目標地点に到達可能な場合はそのまま返す
	if (distance <= maxDistance)
	{
		return targetPos;
	}

	// 最大距離で制限した位置を返す
	direction.NormalizeSelf();
	return currentPos + direction * maxDistance;
}

// 移動速度を制限
float AssaultEnemyBehavior::LimitMovementSpeed(float baseSpeed, float dt)
{
	// 1フレームあたりの移動距離に上限を設定
	return std::min(baseSpeed * dt, maxMoveDistancePerFrame_);
}

// スタック状態かどうか確認
bool AssaultEnemyBehavior::IsStuck(GameObject* owner)
{
	Vector3 currentPos = owner->GetPosition();
	float movement = (currentPos - lastPosition_).Length();

	// 移動量が閾値未満の場合
	if (movement < kStuckMovementThreshold)
	{
		stuckTimer_ += TimeManager::GetInstance().GetGameContext().deltaTime;

		// スタック判定時間を超えた場合
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

// 強制移動（スタック解消用）
void AssaultEnemyBehavior::ForceMovement(GameObject* owner)
{
	// ランダムな方向を生成
	std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
	Vector3 randomDir(dist(rng_), 0, dist(rng_));
	randomDir.NormalizeSelf();

	// 強制的に少し動かす
	float forceMove = moveSpeed_ * kForceMovementSpeedMultiplier * (TimeManager::GetInstance().GetGameContext().deltaTime);
	owner->SetPosition(owner->GetPosition() + randomDir * forceMove);
}