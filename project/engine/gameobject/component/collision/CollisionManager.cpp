#include "CollisionManager.h"
#include <algorithm>

#include "math/AABB.h"
#include "engine/gameobject/component/collision/AABBColliderComponent.h"
#include "engine/gameobject/component/base/ICollisionComponent.h"
#include "engine/gameobject/base/GameObject.h"
#include "base/Logger.h"
#include "imgui/imgui.h"
#include "math/MathUtils.h"

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
				ImGui::Text("Collider %zu: nullptr", i);
			}
			ImGui::Separator();
		}
	}

	ImGui::SeparatorText("Current Collisions");

	ImGui::Text("Registered Colliders: %zu", colliders_.size());
	for (size_t i = 0; i < colliders_.size(); ++i)
	{
		ICollisionComponent* collider = colliders_[i];
		std::string label = std::to_string(i) + ": ";
		if (collider && collider->GetOwner())
		{
			label += collider->GetOwner()->GetTag();
			label += " (" + GetColliderTypeString(collider->GetColliderType()) + ")";
			label += ",  substep: " + std::to_string(collider->UseSubstep());
		}
		else
		{
			label += "nullptr";
		}
		ImGui::Text("%s", label.c_str());
	}

	

	ImGui::End();
#endif

	// 新しい衝突ペアを格納するセット
	std::unordered_set<CollisionPair, CollisionPairHash> newCollisions;

	// 全コライダーの組み合わせで判定
	for (size_t i = 0; i < colliders_.size(); ++i)
	{
		for (size_t j = i + 1; j < colliders_.size(); ++j)
		{
			ICollisionComponent* a = colliders_[i];
			ICollisionComponent* b = colliders_[j];

			bool isHit = false;

			ColliderType typeA = a->GetColliderType();
			ColliderType typeB = b->GetColliderType();

			// 3Dモードの判定
			if (dimension_ == CollisionDimension::Mode3D)
			{
				// AABB同士の衝突判定
				if (typeA == ColliderType::AABB && typeB == ColliderType::AABB)
				{
					// サブステップ判定の使用チェック（高速移動体のすり抜け防止）
					if (a->UseSubstep() || b->UseSubstep())
						isHit = collisionAlgorithm::CheckAABBvsAABBSubstep3D(static_cast<AABBColliderComponent*>(a), static_cast<AABBColliderComponent*>(b));
					else
						isHit = collisionAlgorithm::CheckAABBvsAABB3D(static_cast<AABBColliderComponent*>(a), static_cast<AABBColliderComponent*>(b));
				}
				// OBB同士の衝突判定
				else if (typeA == ColliderType::OBB && typeB == ColliderType::OBB)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = collisionAlgorithm::CheckOBBvsOBBSubstep3D(static_cast<OBBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b));
					else
						isHit = collisionAlgorithm::CheckOBBvsOBB3D(static_cast<OBBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b));
				}
				// AABBとOBBの衝突判定
				else if (typeA == ColliderType::AABB && typeB == ColliderType::OBB)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = collisionAlgorithm::CheckAABBvsOBBSubstep3D(static_cast<AABBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b));
					else
						isHit = collisionAlgorithm::CheckAABBvsOBB3D(static_cast<AABBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b));
				}
				else if (typeA == ColliderType::OBB && typeB == ColliderType::AABB)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = collisionAlgorithm::CheckAABBvsOBBSubstep3D(static_cast<AABBColliderComponent*>(b), static_cast<OBBColliderComponent*>(a));
					else
						isHit = collisionAlgorithm::CheckAABBvsOBB3D(static_cast<AABBColliderComponent*>(b), static_cast<OBBColliderComponent*>(a));
				}
				// Sphere同士の衝突判定
				else if (typeA == ColliderType::Sphere && typeB == ColliderType::Sphere)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = collisionAlgorithm::CheckSpherevsSphereSubstep3D(static_cast<SphereColliderComponent*>(a), static_cast<SphereColliderComponent*>(b));
					else
						isHit = collisionAlgorithm::CheckSpherevsSphere3D(static_cast<SphereColliderComponent*>(a), static_cast<SphereColliderComponent*>(b));
				}
				// SphereとAABBの衝突判定
				else if (typeA == ColliderType::Sphere && typeB == ColliderType::AABB)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = collisionAlgorithm::CheckSpherevsAABBSubstep3D(static_cast<SphereColliderComponent*>(a), static_cast<AABBColliderComponent*>(b));
					else
						isHit = collisionAlgorithm::CheckSpherevsAABB3D(static_cast<SphereColliderComponent*>(a), static_cast<AABBColliderComponent*>(b));
				}
				else if (typeA == ColliderType::AABB && typeB == ColliderType::Sphere)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = collisionAlgorithm::CheckSpherevsAABBSubstep3D(static_cast<SphereColliderComponent*>(b), static_cast<AABBColliderComponent*>(a));
					else
						isHit = collisionAlgorithm::CheckSpherevsAABB3D(static_cast<SphereColliderComponent*>(b), static_cast<AABBColliderComponent*>(a));
				}
				// SphereとOBBの衝突判定
				else if (typeA == ColliderType::Sphere && typeB == ColliderType::OBB)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = collisionAlgorithm::CheckSpherevsOBBSubstep3D(static_cast<SphereColliderComponent*>(a), static_cast<OBBColliderComponent*>(b));
					else
						isHit = collisionAlgorithm::CheckSpherevsOBB3D(static_cast<SphereColliderComponent*>(a), static_cast<OBBColliderComponent*>(b));
				}
				else if (typeA == ColliderType::OBB && typeB == ColliderType::Sphere)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = collisionAlgorithm::CheckSpherevsOBBSubstep3D(static_cast<SphereColliderComponent*>(b), static_cast<OBBColliderComponent*>(a));
					else
						isHit = collisionAlgorithm::CheckSpherevsOBB3D(static_cast<SphereColliderComponent*>(b), static_cast<OBBColliderComponent*>(a));
				}
			}
			// 2Dモードの判定
			else if (dimension_ == CollisionDimension::Mode2D)
			{
				
				// AABB同士の衝突判定
				if (typeA == ColliderType::AABB && typeB == ColliderType::AABB)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = collisionAlgorithm::CheckAABBvsAABBSubstep2D(static_cast<AABBColliderComponent*>(a), static_cast<AABBColliderComponent*>(b), collisionPlane_);
					else
						isHit = collisionAlgorithm::CheckAABBvsAABB2D(static_cast<AABBColliderComponent*>(a), static_cast<AABBColliderComponent*>(b), collisionPlane_);
				}
				// OBB同士の衝突判定
				else if (typeA == ColliderType::OBB && typeB == ColliderType::OBB)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = collisionAlgorithm::CheckOBBvsOBBSubstep2D(static_cast<OBBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b), collisionPlane_);
					else
						isHit = collisionAlgorithm::CheckOBBvsOBB2D(static_cast<OBBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b), collisionPlane_);
				}
				// AABBとOBBの衝突判定
				else if (typeA == ColliderType::AABB && typeB == ColliderType::OBB)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = collisionAlgorithm::CheckAABBvsOBBSubstep2D(static_cast<AABBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b), collisionPlane_);
					else
						isHit = collisionAlgorithm::CheckAABBvsOBB2D(static_cast<AABBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b), collisionPlane_);
				}
				else if (typeA == ColliderType::OBB && typeB == ColliderType::AABB)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = collisionAlgorithm::CheckAABBvsOBBSubstep2D(static_cast<AABBColliderComponent*>(b), static_cast<OBBColliderComponent*>(a), collisionPlane_);
					else
						isHit = collisionAlgorithm::CheckAABBvsOBB2D(static_cast<AABBColliderComponent*>(b), static_cast<OBBColliderComponent*>(a), collisionPlane_);
				}
				// Circle同士の衝突判定
				else if (typeA == ColliderType::Sphere && typeB == ColliderType::Sphere)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = collisionAlgorithm::CheckCirclevsCircleSubstep2D(static_cast<SphereColliderComponent*>(a), static_cast<SphereColliderComponent*>(b), collisionPlane_);
					else
						isHit = collisionAlgorithm::CheckCirclevsCircle2D(static_cast<SphereColliderComponent*>(a), static_cast<SphereColliderComponent*>(b), collisionPlane_);
				}
				// CircleとAABBの衝突判定
				else if (typeA == ColliderType::Sphere && typeB == ColliderType::AABB)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = collisionAlgorithm::CheckCirclevsAABBSubstep2D(static_cast<SphereColliderComponent*>(a), static_cast<AABBColliderComponent*>(b), collisionPlane_);
					else
						isHit = collisionAlgorithm::CheckCirclevsAABB2D(static_cast<SphereColliderComponent*>(a), static_cast<AABBColliderComponent*>(b), collisionPlane_);
				}
				else if (typeA == ColliderType::AABB && typeB == ColliderType::Sphere)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = collisionAlgorithm::CheckCirclevsAABBSubstep2D(static_cast<SphereColliderComponent*>(b), static_cast<AABBColliderComponent*>(a), collisionPlane_);
					else
						isHit = collisionAlgorithm::CheckCirclevsAABB2D(static_cast<SphereColliderComponent*>(b), static_cast<AABBColliderComponent*>(a), collisionPlane_);
				}
				// CircleとOBBの衝突判定
				else if (typeA == ColliderType::Sphere && typeB == ColliderType::OBB)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = collisionAlgorithm::CheckCirclevsOBBSubstep2D(static_cast<SphereColliderComponent*>(a), static_cast<OBBColliderComponent*>(b), collisionPlane_);
					else
						isHit = collisionAlgorithm::CheckCirclevsOBB2D(static_cast<SphereColliderComponent*>(a), static_cast<OBBColliderComponent*>(b), collisionPlane_);
				}
				else if (typeA == ColliderType::OBB && typeB == ColliderType::Sphere)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = collisionAlgorithm::CheckCirclevsOBBSubstep2D(static_cast<SphereColliderComponent*>(b), static_cast<OBBColliderComponent*>(a), collisionPlane_);
					else
						isHit = collisionAlgorithm::CheckCirclevsOBB2D(static_cast<SphereColliderComponent*>(b), static_cast<OBBColliderComponent*>(a), collisionPlane_);
				}
			}

			// 衝突している場合
			if (isHit)
			{
				CollisionPair pair{ a, b };
				newCollisions.insert(pair);
				
				// 衝突した瞬間の処理
				if (!currentCollisions_.contains(pair))
				{
					a->CallOnEnter(b->GetOwner());
					b->CallOnEnter(a->GetOwner());
				}
				else
				{
					// 衝突している間の処理
					a->CallOnStay(b->GetOwner());
					b->CallOnStay(a->GetOwner());
				}
			}
		}
	}

	// 離れた衝突を処理
	for (const auto& pair : currentCollisions_)
	{
		if (!newCollisions.contains(pair))
		{
			// 衝突が離れた時の処理
			pair.a->CallOnExit(pair.b->GetOwner());
			pair.b->CallOnExit(pair.a->GetOwner());
		}
	}

	currentCollisions_ = std::move(newCollisions);
}

void CollisionManager::UpdatePreviousPositions()
{
	for (auto& collider : colliders_)
	{
		collider->SetPreviousPosition(collider->GetOwner()->GetPosition());
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
