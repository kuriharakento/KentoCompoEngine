#include "CollisionManager.h"
#include <algorithm>

#include "math/AABB.h"
#include "application/GameObject/component/collision/AABBColliderComponent.h"
#include "application/GameObject/component/base/ICollisionComponent.h"
#include "application/GameObject/base/GameObject.h"
#include "base/Logger.h"
#include "imgui/imgui.h"
#include "math/MathUtils.h"

CollisionManager* CollisionManager::instance_ = nullptr; // シングルトンインスタンス

CollisionManager* CollisionManager::GetInstance()
{
	if (instance_ == nullptr)
	{
		instance_ = new CollisionManager();
	}
	return instance_;
}

void CollisionManager::Register(ICollisionComponent* collider)
{
	colliders_.push_back(collider);
}

void CollisionManager::Unregister(ICollisionComponent* collider)
{
	// currentCollisions_ から該当コライダーを含むペアを削除
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
#ifdef _DEBUG
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

	std::unordered_set<CollisionPair, CollisionPairHash> newCollisions;

	for (size_t i = 0; i < colliders_.size(); ++i)
	{
		for (size_t j = i + 1; j < colliders_.size(); ++j)
		{
			ICollisionComponent* a = colliders_[i];
			ICollisionComponent* b = colliders_[j];

			bool isHit = false;

			// 衝突判定のディスパッチ
			ColliderType typeA = a->GetColliderType();
			ColliderType typeB = b->GetColliderType();

			// 判定次元モード判定
			//====== 3Dモード ======
			if (dimension_ == CollisionDimension::Mode3D)
			{
				// AABB同士の衝突判定
				if (typeA == ColliderType::AABB && typeB == ColliderType::AABB)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = CollisionAlgorithm::CheckAABBvsAABBSubstep3D(static_cast<AABBColliderComponent*>(a), static_cast<AABBColliderComponent*>(b));
					else
						isHit = CollisionAlgorithm::CheckAABBvsAABB3D(static_cast<AABBColliderComponent*>(a), static_cast<AABBColliderComponent*>(b));
				}
				// OBB同士の衝突判定
				else if (typeA == ColliderType::OBB && typeB == ColliderType::OBB)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = CollisionAlgorithm::CheckOBBvsOBBSubstep3D(static_cast<OBBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b));
					else
						isHit = CollisionAlgorithm::CheckOBBvsOBB3D(static_cast<OBBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b));
				}
				// AABBとOBBの衝突判定
				else if (typeA == ColliderType::AABB && typeB == ColliderType::OBB)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = CollisionAlgorithm::CheckAABBvsOBBSubstep3D(static_cast<AABBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b));
					else
						isHit = CollisionAlgorithm::CheckAABBvsOBB3D(static_cast<AABBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b));
				}
				else if (typeA == ColliderType::OBB && typeB == ColliderType::AABB)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = CollisionAlgorithm::CheckAABBvsOBBSubstep3D(static_cast<AABBColliderComponent*>(b), static_cast<OBBColliderComponent*>(a));
					else
						isHit = CollisionAlgorithm::CheckAABBvsOBB3D(static_cast<AABBColliderComponent*>(b), static_cast<OBBColliderComponent*>(a));
				}
				// Sphere同士の衝突判定
				else if (typeA == ColliderType::Sphere && typeB == ColliderType::Sphere)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = CollisionAlgorithm::CheckSpherevsSphereSubstep3D(static_cast<SphereColliderComponent*>(a), static_cast<SphereColliderComponent*>(b));
					else
						isHit = CollisionAlgorithm::CheckSpherevsSphere3D(static_cast<SphereColliderComponent*>(a), static_cast<SphereColliderComponent*>(b));
				}
				// SphereとAABBの衝突判定
				else if (typeA == ColliderType::Sphere && typeB == ColliderType::AABB)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = CollisionAlgorithm::CheckSpherevsAABBSubstep3D(static_cast<SphereColliderComponent*>(a), static_cast<AABBColliderComponent*>(b));
					else
						isHit = CollisionAlgorithm::CheckSpherevsAABB3D(static_cast<SphereColliderComponent*>(a), static_cast<AABBColliderComponent*>(b));
				}
				else if (typeA == ColliderType::AABB && typeB == ColliderType::Sphere)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = CollisionAlgorithm::CheckSpherevsAABBSubstep3D(static_cast<SphereColliderComponent*>(b), static_cast<AABBColliderComponent*>(a));
					else
						isHit = CollisionAlgorithm::CheckSpherevsAABB3D(static_cast<SphereColliderComponent*>(b), static_cast<AABBColliderComponent*>(a));
				}
				// SphereとOBBの衝突判定
				else if (typeA == ColliderType::Sphere && typeB == ColliderType::OBB)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = CollisionAlgorithm::CheckSpherevsOBBSubstep3D(static_cast<SphereColliderComponent*>(a), static_cast<OBBColliderComponent*>(b));
					else
						isHit = CollisionAlgorithm::CheckSpherevsOBB3D(static_cast<SphereColliderComponent*>(a), static_cast<OBBColliderComponent*>(b));
				}
				else if (typeA == ColliderType::OBB && typeB == ColliderType::Sphere)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = CollisionAlgorithm::CheckSpherevsOBBSubstep3D(static_cast<SphereColliderComponent*>(b), static_cast<OBBColliderComponent*>(a));
					else
						isHit = CollisionAlgorithm::CheckSpherevsOBB3D(static_cast<SphereColliderComponent*>(b), static_cast<OBBColliderComponent*>(a));
				}
			}
			//===== 2Dモード =====
			else if (dimension_ == CollisionDimension::Mode2D)
			{
				
				// AABB同士の衝突判定
				if (typeA == ColliderType::AABB && typeB == ColliderType::AABB)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = CollisionAlgorithm::CheckAABBvsAABBSubstep2D(static_cast<AABBColliderComponent*>(a), static_cast<AABBColliderComponent*>(b), collisionPlane_);
					else
						isHit = CollisionAlgorithm::CheckAABBvsAABB2D(static_cast<AABBColliderComponent*>(a), static_cast<AABBColliderComponent*>(b), collisionPlane_);
				}
				// OBB同士の衝突判定
				else if (typeA == ColliderType::OBB && typeB == ColliderType::OBB)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = CollisionAlgorithm::CheckOBBvsOBBSubstep2D(static_cast<OBBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b), collisionPlane_);
					else
						isHit = CollisionAlgorithm::CheckOBBvsOBB2D(static_cast<OBBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b), collisionPlane_);
				}
				// AABBとOBBの衝突判定
				else if (typeA == ColliderType::AABB && typeB == ColliderType::OBB)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = CollisionAlgorithm::CheckAABBvsOBBSubstep2D(static_cast<AABBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b), collisionPlane_);
					else
						isHit = CollisionAlgorithm::CheckAABBvsOBB2D(static_cast<AABBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b), collisionPlane_);
				}
				else if (typeA == ColliderType::OBB && typeB == ColliderType::AABB)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = CollisionAlgorithm::CheckAABBvsOBBSubstep2D(static_cast<AABBColliderComponent*>(b), static_cast<OBBColliderComponent*>(a), collisionPlane_);
					else
						isHit = CollisionAlgorithm::CheckAABBvsOBB2D(static_cast<AABBColliderComponent*>(b), static_cast<OBBColliderComponent*>(a), collisionPlane_);
				}
				// Circle同士の衝突判定
				else if (typeA == ColliderType::Sphere && typeB == ColliderType::Sphere)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = CollisionAlgorithm::CheckCirclevsCircleSubstep2D(static_cast<SphereColliderComponent*>(a), static_cast<SphereColliderComponent*>(b), collisionPlane_);
					else
						isHit = CollisionAlgorithm::CheckCirclevsCircle2D(static_cast<SphereColliderComponent*>(a), static_cast<SphereColliderComponent*>(b), collisionPlane_);
				}
				// CircleとAABBの衝突判定
				else if (typeA == ColliderType::Sphere && typeB == ColliderType::AABB)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = CollisionAlgorithm::CheckCirclevsAABBSubstep2D(static_cast<SphereColliderComponent*>(a), static_cast<AABBColliderComponent*>(b), collisionPlane_);
					else
						isHit = CollisionAlgorithm::CheckCirclevsAABB2D(static_cast<SphereColliderComponent*>(a), static_cast<AABBColliderComponent*>(b), collisionPlane_);
				}
				else if (typeA == ColliderType::AABB && typeB == ColliderType::Sphere)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = CollisionAlgorithm::CheckCirclevsAABBSubstep2D(static_cast<SphereColliderComponent*>(b), static_cast<AABBColliderComponent*>(a), collisionPlane_);
					else
						isHit = CollisionAlgorithm::CheckCirclevsAABB2D(static_cast<SphereColliderComponent*>(b), static_cast<AABBColliderComponent*>(a), collisionPlane_);
				}
				// CircleとOBBの衝突判定
				else if (typeA == ColliderType::Sphere && typeB == ColliderType::OBB)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = CollisionAlgorithm::CheckCirclevsOBBSubstep2D(static_cast<SphereColliderComponent*>(a), static_cast<OBBColliderComponent*>(b), collisionPlane_);
					else
						isHit = CollisionAlgorithm::CheckCirclevsOBB2D(static_cast<SphereColliderComponent*>(a), static_cast<OBBColliderComponent*>(b), collisionPlane_);
				}
				else if (typeA == ColliderType::OBB && typeB == ColliderType::Sphere)
				{
					if (a->UseSubstep() || b->UseSubstep())
						isHit = CollisionAlgorithm::CheckCirclevsOBBSubstep2D(static_cast<SphereColliderComponent*>(b), static_cast<OBBColliderComponent*>(a), collisionPlane_);
					else
						isHit = CollisionAlgorithm::CheckCirclevsOBB2D(static_cast<SphereColliderComponent*>(b), static_cast<OBBColliderComponent*>(a), collisionPlane_);
				}
			}

			// 衝突している場合
			if (isHit)
			{
				CollisionPair pair{ a, b };
				newCollisions.insert(pair);
				//衝突した瞬間の処理
				if (!currentCollisions_.contains(pair))
				{
					a->CallOnEnter(b->GetOwner());
					b->CallOnEnter(a->GetOwner());
				}
				else
				{
					//衝突している間の処理
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
			//衝突が離れた場合の処理
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
#ifdef _DEBUG   // デバッグビルド時のみログを出力
	std::string tagA = a->GetOwner()->GetTag();
	std::string tagB = b->GetOwner()->GetTag();
	std::string typeAString = GetColliderTypeString(a->GetColliderType());
	std::string typeBString = GetColliderTypeString(b->GetColliderType());

	Logger::Log("| Collision " + phase + " " +
				(phase == "Exit" ? "<-" : (phase == "Enter" ? "->" : "=="))
				+ " | " + tagA + ": " + typeAString + ", " + tagB + ": " + typeBString + "\n");
#endif
}
