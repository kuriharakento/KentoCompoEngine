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

KnifeEnemyBehavior::KnifeEnemyBehavior(GameObject* target, GameObject* knife)
	: target_(target), knife_(knife)
{
	std::random_device rd;
	rng_ = std::mt19937(rd());
	BuildBehaviorTree();
}

void KnifeEnemyBehavior::Update(GameObject* owner)
{
	float deltaTime = TimeManager::GetInstance().GetDeltaTime();
	if (attackCooldown_ > 0) attackCooldown_ -= deltaTime;

	ImGui::Begin("KnifeBehavior");
	{
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

	behaviorTree_->Tick();
}

void KnifeEnemyBehavior::BuildBehaviorTree()
{
	auto root = std::make_unique<SelectorNode>();

	// 1. 近接攻撃（ターゲット発見＆攻撃範囲内）
	auto attackSeq = std::make_unique<SequenceNode>();
	attackSeq->AddChild(std::make_unique<ConditionNode>([this](Blackboard& bb) {
		return bb.Get<bool>("IsTargetVisible") && bb.Get<bool>("IsInAttackRange");
														}));
	attackSeq->AddChild(std::make_unique<ActionNode>(
		"Attack",
		[this](Blackboard& bb) {
			auto owner = bb.Get<GameObject*>("Owner");
			if (AttackAction(owner))
			{
				return NodeStatus::Success;
			}
			return NodeStatus::Running;
		}));
	root->AddChild(std::move(attackSeq));

	// 2. 追従（発見済みだが攻撃距離ではない）
	auto chaseSeq = std::make_unique<SequenceNode>();
	chaseSeq->AddChild(std::make_unique<ConditionNode>([this](Blackboard& bb) {
		return bb.Get<bool>("IsTargetVisible");
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

	// 3. パトロール（未発見）
	auto patrolSeq = std::make_unique<SequenceNode>();
	patrolSeq->AddChild(std::make_unique<ConditionNode>([this](Blackboard& bb) {
		return !bb.Get<bool>("IsTargetVisible");
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
	float moveDistance = LimitMovementSpeed(moveSpeed_ * patrolSpeed_, TimeManager::GetInstance().GetDeltaTime());
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
	float moveDistance = LimitMovementSpeed(moveSpeed_, TimeManager::GetInstance().GetDeltaTime());
	owner->SetPosition(owner->GetPosition() + dir * moveDistance);
	float angle = atan2(dir.x, dir.z);
	owner->SetRotation(Vector3(0, angle, 0));
	return false; // まだ到達してない
}

bool KnifeEnemyBehavior::AttackAction(GameObject* owner)
{
	float deltaTime = TimeManager::GetInstance().GetDeltaTime();
	if (!target_ || !knife_) return true;
	Vector3 ownerPos = owner->GetPosition();
	if ((target_->GetPosition() - ownerPos).Length() > attackRange_ * 1.5f)
		return true;
	if (attackCooldown_ > 0) { attackCooldown_ -= deltaTime; return false; }

	static float elapsed = 0.0f;
	static bool attacking = false;
	static Vector3 swingCenter;
	static Vector3 facingDir;
	static Vector3 right;
	static Vector3 knifeBaseOffset;
	const float animTime = 0.2f;
	const float forwardOffset = 2.0f;   // 前方への距離
	const float swingWidth = 4.0f;      // 横幅（矩形範囲）

	if (!attacking)
	{
		swingCenter = ownerPos;
		Vector3 rotation = owner->GetRotation();
		float yaw = rotation.y;
		facingDir = Vector3(std::sinf(yaw * (std::numbers::pi / 180.0f)), 0, std::cosf(yaw * (std::numbers::pi / 180.0f)));
		if (facingDir.Length() < 0.1f) facingDir = Vector3(0, 0, 1);
		facingDir.NormalizeSelf();
		Vector3 up(0, 1, 0);
		right = up.Cross(facingDir, up); right.NormalizeSelf();
		knifeBaseOffset = knife_->GetPosition() - (swingCenter + facingDir * forwardOffset);
		attacking = true;
		elapsed = 0.0f;
	}
	elapsed += deltaTime;
	float t = std::min(elapsed / animTime, 1.0f);

	// ownerを少しだけ前進（演出用、不要なら削除）
	float dashDistance = 0.6f;
	owner->SetPosition(swingCenter + facingDir * dashDistance * t);

	// ナイフを前方＋左右に移動（ヨネWの矩形範囲をなぞる）
	float swingStart = -swingWidth / 2.0f;
	float swingEnd = swingWidth / 2.0f;
	float swingX = swingStart + (swingEnd - swingStart) * t;
	Vector3 swingPos = swingCenter + facingDir * forwardOffset + right * swingX + knifeBaseOffset;
	knife_->SetPosition(swingPos);

	if (t >= 1.0f)
	{
		knife_->SetPosition(swingCenter + facingDir * forwardOffset + knifeBaseOffset);
		attacking = false;
		elapsed = 0.0f;
		attackCooldown_ = attackInterval_;
		return true;
	}
	return false;
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