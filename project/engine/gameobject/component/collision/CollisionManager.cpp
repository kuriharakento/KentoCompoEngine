#include "CollisionManager.h"
#include <algorithm>
#include <unordered_set>

#include "math/AABB.h"
#include "engine/gameobject/component/collision/AABBColliderComponent.h"
#include "engine/gameobject/component/collision/OBBColliderComponent.h"
#include "engine/gameobject/component/collision/SphereColliderComponent.h"
#include "engine/gameobject/component/collision/RayColliderComponent.h"
#include "engine/gameobject/component/base/ICollisionComponent.h"
#include "engine/gameobject/base/GameObject.h"
#include "base/Logger.h"
#include "imgui/imgui.h"
#include "math/MathUtils.h"
#include "engine/gameobject/component/collision/CollisionAlgorithm.h"

std::unique_ptr<CollisionManager> CollisionManager::instance_ = nullptr;

CollisionManager* CollisionManager::GetInstance()
{
	if (instance_ == nullptr)
	{
		instance_.reset(new CollisionManager());
	}
	return instance_.get();
}
void CollisionManager::Finalize()
{
	colliders_.clear();
	currentCollisions_.clear();
	instance_.reset();
}

void CollisionManager::Register(ICollisionComponent* collider)
{
	colliders_.push_back(collider);
}

void CollisionManager::Unregister(ICollisionComponent* collider)
{
	// このコライダーを含む衝突ペアを全て削除
	for (auto it = currentCollisions_.begin(); it != currentCollisions_.end(); )
	{
		if (it->a == collider || it->b == collider)
		{
			it = currentCollisions_.erase(it);
		}
		else
		{
			++it;
		}
	}

	colliders_.erase(std::remove(colliders_.begin(), colliders_.end(), collider), colliders_.end());
}

void CollisionManager::CheckCollisions()
{
#ifdef USE_IMGUI
	ImGui::Begin("CollisionManager Colliders");

	ImGui::SeparatorText("Colliders");
	if (ImGui::CollapsingHeader("List"))
	{
		for (size_t i = 0; i < colliders_.size(); ++i)
		{
			ICollisionComponent* collider = colliders_[i];
			if (collider && collider->GetOwner())
			{
				ImGui::Text("Collider %zu: %s", i, collider->GetOwner()->GetTag().c_str());
				ImGui::Text("Position: (%.2f, %.2f, %.2f)", collider->GetOwner()->GetPosition().x, collider->GetOwner()->GetPosition().y, collider->GetOwner()->GetPosition().z);
				ImGui::Text("Previous Position: (%.2f, %.2f, %.2f)", collider->GetPreviousPosition().x, collider->GetPreviousPosition().y, collider->GetPreviousPosition().z);
			}
			else
			{
				ImGui::Text("Collider %zu: [Invalid Owner]", i);
			}
		}
	}

	ImGui::SeparatorText("Statistics");
	ImGui::Text("Total Calliders: %zu", colliders_.size());
	ImGui::Text("Active Collisions: %zu", currentCollisions_.size());

	ImGui::End();
#endif

	// 新しい衝突ペアを格納するセット
	std::unordered_set<CollisionPair, CollisionPairHash> newCollisions;

	// 全てのペアに対して判定を行う
	for (size_t i = 0; i < colliders_.size(); ++i)
	{
		for (size_t j = i + 1; j < colliders_.size(); ++j)
		{
			ICollisionComponent* a = colliders_[i];
			ICollisionComponent* b = colliders_[j];

			// 両方のオブジェクトがアクティブな場合のみ判定
			if (!a->GetOwner()->IsActive() || !b->GetOwner()->IsActive())
			{
				continue;
			}

			bool isHit = false;

			// 型に応じて判定関数を呼び分け
			if (a->GetColliderType() == ColliderType::AABB && b->GetColliderType() == ColliderType::AABB)
			{
				isHit = collisionAlgorithm::CheckAABBvsAABBSubstep3D(static_cast<AABBColliderComponent*>(a), static_cast<AABBColliderComponent*>(b));
			}
			else if (a->GetColliderType() == ColliderType::OBB && b->GetColliderType() == ColliderType::OBB)
			{
				isHit = collisionAlgorithm::CheckOBBvsOBBSubstep3D(static_cast<OBBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b));
			}
			else if (a->GetColliderType() == ColliderType::Sphere && b->GetColliderType() == ColliderType::Sphere)
			{
				isHit = collisionAlgorithm::CheckSpherevsSphereSubstep3D(static_cast<SphereColliderComponent*>(a), static_cast<SphereColliderComponent*>(b));
			}
			else if (a->GetColliderType() == ColliderType::Sphere && b->GetColliderType() == ColliderType::AABB)
			{
				isHit = collisionAlgorithm::CheckSpherevsAABBSubstep3D(static_cast<SphereColliderComponent*>(a), static_cast<AABBColliderComponent*>(b));
			}
			else if (a->GetColliderType() == ColliderType::AABB && b->GetColliderType() == ColliderType::Sphere)
			{
				isHit = collisionAlgorithm::CheckSpherevsAABBSubstep3D(static_cast<SphereColliderComponent*>(b), static_cast<AABBColliderComponent*>(a));
			}
			else if (a->GetColliderType() == ColliderType::Sphere && b->GetColliderType() == ColliderType::OBB)
			{
				isHit = collisionAlgorithm::CheckSpherevsOBBSubstep3D(static_cast<SphereColliderComponent*>(a), static_cast<OBBColliderComponent*>(b));
			}
			else if (a->GetColliderType() == ColliderType::OBB && b->GetColliderType() == ColliderType::Sphere)
			{
				isHit = collisionAlgorithm::CheckSpherevsOBBSubstep3D(static_cast<SphereColliderComponent*>(b), static_cast<OBBColliderComponent*>(a));
			}
			else if (a->GetColliderType() == ColliderType::AABB && b->GetColliderType() == ColliderType::OBB)
			{
				isHit = collisionAlgorithm::CheckAABBvsOBBSubstep3D(static_cast<AABBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b));
			}
			else if (a->GetColliderType() == ColliderType::OBB && b->GetColliderType() == ColliderType::AABB)
			{
				isHit = collisionAlgorithm::CheckAABBvsOBBSubstep3D(static_cast<AABBColliderComponent*>(b), static_cast<OBBColliderComponent*>(a));
			}

			if (isHit)
			{
				newCollisions.insert({ a, b });
			}
		}
	}

	// 衝突状態の変化を検出し、コールバックを呼び出す
	for (auto& pair : newCollisions)
	{
		if (currentCollisions_.find(pair) == currentCollisions_.end())
		{
			// 新規衝突 (OnEnter)
			pair.a->CallOnEnter(pair.b->GetOwner());
			pair.b->CallOnEnter(pair.a->GetOwner());
			LogCollision("Enter", pair.a, pair.b);
		}
		else
		{
			// 継続衝突 (OnStay)
			pair.a->CallOnStay(pair.b->GetOwner());
			pair.b->CallOnStay(pair.a->GetOwner());
		}
	}

	for (auto& pair : currentCollisions_)
	{
		if (newCollisions.find(pair) == newCollisions.end())
		{
			// 衝突終了 (OnExit)
			pair.a->CallOnExit(pair.b->GetOwner());
			pair.b->CallOnExit(pair.a->GetOwner());
			LogCollision("Exit", pair.a, pair.b);
		}
	}

	currentCollisions_ = std::move(newCollisions);
}

void CollisionManager::UpdatePreviousPositions()
{
	for (auto& collider : colliders_)
	{
		// トンネリング(CCD)判定を正しく行うため、ワールド座標で保持する
		collider->SetPreviousPosition(MathUtils::GetTranslateFromMatrix(collider->GetOwner()->GetWorldMatrix()));
	}
}

std::string CollisionManager::GetColliderTypeString(ColliderType type) const
{
	switch (type)
	{
	case ColliderType::AABB:
		return "AABB";
	case ColliderType::Sphere:
		return "Sphere";
	case ColliderType::OBB:
		return "OBB";
	case ColliderType::Ray:
		return "Ray";
	}
	return "Unknown";
}

void CollisionManager::LogCollision(const std::string& phase, const ICollisionComponent* a, const ICollisionComponent* b)
{
#ifdef _DEBUG
	std::string tagA = a->GetOwner()->GetTag();
	std::string tagB = b->GetOwner()->GetTag();
	std::string typeAString = GetColliderTypeString(a->GetColliderType());
	std::string typeBString = GetColliderTypeString(b->GetColliderType());

	Logger::Log("| Collision " + phase + " " +
				(phase == "Exit" ? "<-" : (phase == "Enter" ? "->" : "=="))
				+ " | " + tagA + ": " + typeAString + ", " + tagB + ": " + typeBString + "\n");
#endif
}
