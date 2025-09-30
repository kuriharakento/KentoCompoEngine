#include "KnifeEnemyBehavior.h"
#include "application/GameObject/base/GameObject.h"
#include "application/GameObject/Combatable/weapon/Knife.h"
#include "time/TimeManager.h"
#include <random>
#include <cmath>

#include "application/GameObject/Combatable/character/enemy/base/Node/ActionNode.h"
#include "application/GameObject/Combatable/character/enemy/base/Node/ConditionNode.h"
#include "application/GameObject/combatable/character/enemy/base/Node/NodeUtils.h"
#include "application/GameObject/Combatable/character/enemy/base/Node/SelectorNode.h"
#include "application/GameObject/Combatable/character/enemy/base/Node/SequenceNode.h"
#include "imgui/imgui.h"
#include "math/Easing.h"

KnifeEnemyBehavior::KnifeEnemyBehavior(GameObject* target, GameObject* rightArm, GameObject* leftArm, GameObject* knife)
	: target_(target), rightArm_(rightArm), leftArm_(leftArm), knife_(knife)
{
	std::random_device rd;
	rng_ = std::mt19937(rd());

	// 初期回転値を設定
	rightArmInitialRotation_ = rightArm_->GetRotation();
	rightArmInitialPosition_ = rightArm_->GetPosition();
	leftArmInitialRotation_ = leftArm_->GetRotation();
	knifeInitialRotation_ = knife_->GetRotation();
	knifeInitialPosition_ = knife_->GetPosition();

	BuildBehaviorTree();
}

void KnifeEnemyBehavior::Update(GameObject* owner)
{
	float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;
	if (attackCooldown_ > 0)
	{
		attackCooldown_ -= deltaTime;
	}

	// 腕のオブジェクトを取得
	if (!rightArm_)
	{
		rightArm_ = owner->GetChild(GameObjectTag::Character::KnifeEnemyRightArm);
	}
	if (!leftArm_)
	{
		leftArm_ = owner->GetChild(GameObjectTag::Character::KnifeEnemyLeftArm);
	}

	// 攻撃モーションの更新
	if (isAttacking_)
	{
		UpdateAttackMotion(owner, deltaTime);
	}

	ImGui::Begin("KnifeBehavior");
	{
		ImGui::Text("Attacking: %s", isAttacking_ ? "true" : "false");
		ImGui::Text("Attack Progress: %.2f", attackProgress_);
		ImGui::Text("Attack Cooldown: %.2f", attackCooldown_);
		ImGui::Text("Target Visible: %s", IsTargetVisible(owner) ? "true" : "false");
		ImGui::Text("In Attack Range: %s", IsInAttackRange(owner) ? "true" : "false");

		if (ImGui::TreeNode("BehaviorTree"))
		{
			if (behaviorTree_)
				NodeUtils::DrawBTNodeImGui(behaviorTree_->GetRoot());
			ImGui::TreePop();
		}
	}
	ImGui::End();

	// Blackboardへ情報セット
	auto& bb = behaviorTree_->GetBlackboard();
	bb.Set<GameObject*>("Owner", owner);
	bb.Set<GameObject*>("Target", target_);
	bb.Set<Vector3>("TargetPosition", target_ ? target_->GetPosition() : Vector3());
	bb.Set<bool>("IsTargetVisible", IsTargetVisible(owner));
	bb.Set<bool>("IsInAttackRange", IsInAttackRange(owner));
	bb.Set<float>("AttackCooldown", attackCooldown_);
	bb.Set<bool>("IsAttacking", isAttacking_);

	behaviorTree_->Tick();
}

void KnifeEnemyBehavior::BuildBehaviorTree()
{
	auto root = std::make_unique<SelectorNode>();

	// 1. 近接攻撃（ターゲット発見＆攻撃範囲内＆攻撃可能状態）
	auto attackSeq = std::make_unique<SequenceNode>();
	attackSeq->AddChild(std::make_unique<ConditionNode>([this](Blackboard& bb) {
		return bb.Get<bool>("IsTargetVisible") &&
			bb.Get<bool>("IsInAttackRange") &&
			bb.Get<float>("AttackCooldown") <= 0.0f &&
			!bb.Get<bool>("IsAttacking");
														}));
	attackSeq->AddChild(std::make_unique<ActionNode>(
		"Attack",
		[this](Blackboard& bb) {
			auto owner = bb.Get<GameObject*>("Owner");
			if (AttackAction(owner))
			{
				return NodeStatus::Success;
			}
			// 攻撃範囲外
			if (!IsInAttackRange(owner))
			{
				return NodeStatus::Failure;
			}
			return NodeStatus::Running;
		}));
	root->AddChild(std::move(attackSeq));

	// 2. 追従（発見済み＆攻撃中でない）
	auto chaseSeq = std::make_unique<SequenceNode>();
	chaseSeq->AddChild(std::make_unique<ConditionNode>([this](Blackboard& bb) {
		return bb.Get<bool>("IsTargetVisible") && !bb.Get<bool>("IsAttacking");
													   }));
	chaseSeq->AddChild(std::make_unique<ActionNode>(
		"Chase",
		[this](Blackboard& bb) {
			auto owner = bb.Get<GameObject*>("Owner");
			if (ChaseAction(owner))
			{
				return NodeStatus::Success;
			}
			return NodeStatus::Running;
		}));
	root->AddChild(std::move(chaseSeq));

	// 3. パトロール（未発見かつ攻撃中でない）
	auto patrolSeq = std::make_unique<SequenceNode>();
	patrolSeq->AddChild(std::make_unique<ConditionNode>([this](Blackboard& bb) {
		return !bb.Get<bool>("IsTargetVisible") && !bb.Get<bool>("IsAttacking");
														}));
	patrolSeq->AddChild(std::make_unique<ActionNode>(
		"Patrol",
		[this](Blackboard& bb) {
			auto owner = bb.Get<GameObject*>("Owner");
			if (PatrolAction(owner))
			{
				return NodeStatus::Success;
			}
			return NodeStatus::Running;
		}));
	root->AddChild(std::move(patrolSeq));

	// 4. Idle
	root->AddChild(std::make_unique<ActionNode>(
		"Idle",
		[this](Blackboard& bb) {
			auto owner = bb.Get<GameObject*>("Owner");
			IdleAction(owner);
			return NodeStatus::Running;
		}));

	behaviorTree_ = std::make_unique<BehaviorTree>(std::move(root));
}

void KnifeEnemyBehavior::IdleAction(GameObject* owner)
{
	// 何もしない
}

bool KnifeEnemyBehavior::PatrolAction(GameObject* owner)
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
		return true; // 到達したらSuccess
	}
	dir.NormalizeSelf();
	float moveDistance = LimitMovementSpeed(moveSpeed_ * patrolSpeed_, TimeManager::GetInstance().GetGameContext().deltaTime);
	owner->SetPosition(owner->GetPosition() + dir * moveDistance);
	float angle = atan2(dir.x, dir.z);
	owner->SetRotation(Vector3(0, angle, 0));
	return false; // まだ到達してない
}

bool KnifeEnemyBehavior::ChaseAction(GameObject* owner)
{
	if (!target_) return true; // 目標消失時は終了
	Vector3 targetPos = target_->GetPosition();
	Vector3 dir = targetPos - owner->GetPosition();
	float dist = dir.Length();
	if (dist < attackRange_)
	{
		return true; // 攻撃レンジに入ったらSuccess
	}
	dir.NormalizeSelf();
	float moveDistance = LimitMovementSpeed(moveSpeed_, TimeManager::GetInstance().GetGameContext().deltaTime);
	owner->SetPosition(owner->GetPosition() + dir * moveDistance);
	float angle = atan2(dir.x, dir.z);
	owner->SetRotation(Vector3(0, angle, 0));
	return false; // まだ到達してない
}

bool KnifeEnemyBehavior::AttackAction(GameObject* owner)
{
	// 攻撃範囲外なら即Failureを返してChaseにBTが遷移
	if (!IsInAttackRange(owner))
	{
		isAttacking_ = false;
		attackProgress_ = 0.0f;
		knife_->SetPosition(knifeInitialPosition_);
		knife_->SetRotation(knifeInitialRotation_);
		rightArm_->SetPosition(rightArmInitialPosition_);
		rightArm_->SetRotation(rightArmInitialRotation_);
		return false; // Failureを返す
	}

	// 既に攻撃中の場合は攻撃の完了を待つ
	if (isAttacking_)
	{
		// 攻撃完了
		if (attackProgress_ >= 1.0f)
		{
			isAttacking_ = false;
			attackProgress_ = 0.0f;
			knife_->SetPosition(knifeInitialPosition_);
			knife_->SetRotation(knifeInitialRotation_);
			rightArm_->SetPosition(rightArmInitialPosition_);
			rightArm_->SetRotation(rightArmInitialRotation_);
			return true; // Success
		}
		return false; // Running
	}

	// クールダウン中なら攻撃不可
	if (attackCooldown_ > 0.0f)
	{
		return false; // Running
	}

	// ターゲットの方向を向く
	if (target_)
	{
		Vector3 targetPos = target_->GetPosition();
		Vector3 direction = targetPos - owner->GetPosition();
		direction.y = 0.0f; // Y軸回転のみ
		direction.NormalizeSelf();

		float angle = atan2(direction.x, direction.z);
		owner->SetRotation(Vector3(0, angle, 0));
	}

	// 攻撃開始
	isAttacking_ = true;
	attackProgress_ = 0.0f;
	hasHitTarget_ = false;
	attackCooldown_ = attackInterval_;

	return false; // Running
}

bool KnifeEnemyBehavior::IsTargetVisible(GameObject* owner)
{
	if (!target_) return false;
	Vector3 targetPos = target_->GetPosition();
	Vector3 direction = targetPos - owner->GetPosition();
	float distance = direction.Length();
	return (distance <= detectionRange_);
}

bool KnifeEnemyBehavior::IsInAttackRange(GameObject* owner)
{
	if (!target_) return false;
	Vector3 targetPos = target_->GetPosition();
	Vector3 direction = targetPos - owner->GetPosition();
	float distance = direction.Length();
	return (distance <= attackRange_);
}

void KnifeEnemyBehavior::InitializePatrolPoints(const Vector3& centerPoint, float radius)
{
	patrolPoints_.clear();
	const int numPoints = 8;
	for (int i = 0; i < numPoints; i++)
	{
		float angle = (i * 2.0f * 3.14159f) / numPoints;
		float x = centerPoint.x + radius * std::cos(angle);
		float z = centerPoint.z + radius * std::sin(angle);
		patrolPoints_.push_back(Vector3(x, centerPoint.y, z));
	}
	std::random_device rd;
	rng_ = std::mt19937(rd());
	currentPatrolIndex_ = std::uniform_int_distribution<int>(0, numPoints - 1)(rng_);
	patrolInitialized_ = true;
}

float KnifeEnemyBehavior::LimitMovementSpeed(float baseSpeed, float dt)
{
	return std::min(baseSpeed * dt, 0.25f);
}

void KnifeEnemyBehavior::UpdateAttackMotion(GameObject* owner, float deltaTime)
{
	if (!isAttacking_) return;


	// 攻撃中に攻撃範囲外なら即中断・初期化
	if (!IsInAttackRange(owner) || attackProgress_ >= 1.0f)
	{
		isAttacking_ = false;
		attackProgress_ = 0.0f;
		knife_->SetPosition(knifeInitialPosition_);
		knife_->SetRotation(knifeInitialRotation_);
		rightArm_->SetPosition(rightArmInitialPosition_);
		rightArm_->SetRotation(rightArmInitialRotation_);
		return;
	}

	attackProgress_ += deltaTime / attackDuration_;
	float t = std::clamp(attackProgress_, 0.0f, 1.0f);
	float easeT = EaseInOutExpo(t); // イージングで強弱

	// 例えば左から右へ（yStart→yEnd）
	float yStart = -1.2f, yEnd = 2.5f;
	float y = yStart + (yEnd - yStart) * easeT;

	Vector3 attackRotation;
	attackRotation.x = -std::numbers::pi_v<float> / 2.0; // -90度
	attackRotation.y = y;
	attackRotation.z = 0.0;

	knife_->SetPosition({ -4.3f, -0.6f, 0.1f });
	knife_->SetRotation({ 0.0f, -1.57f, 0.0f });
	Vector3 rightArmPos = rightArmInitialPosition_;
	rightArmPos.z = 1.5f; // ナイフを持つ手を前に出す
	rightArm_->SetPosition(rightArmPos);
	rightArm_->SetRotation(attackRotation);
}