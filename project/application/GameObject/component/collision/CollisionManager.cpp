#include "CollisionManager.h"
#include <algorithm>

#include "math/AABB.h"
#include "application/GameObject/component/collision/AABBColliderComponent.h"
#include "application/GameObject/component/base/ICollisionComponent.h"
#include "application/GameObject/base/GameObject.h"
#include "base/Logger.h"
#include "imgui/imgui.h"
#include "math/MathUtils.h"

// シングルトンインスタンス
CollisionManager* CollisionManager::instance_ = nullptr;

/**
 * @brief シングルトンインスタンスを取得
 */
CollisionManager* CollisionManager::GetInstance()
{
	// 初回呼び出し時にインスタンスを生成
	if (instance_ == nullptr)
	{
		instance_ = new CollisionManager();
	}
	return instance_;
}

/**
 * @brief コライダーを登録
 */
void CollisionManager::Register(ICollisionComponent* collider)
{
	// コライダーリストに追加
	colliders_.push_back(collider);
}

/**
 * @brief コライダーを登録解除
 */
void CollisionManager::Unregister(ICollisionComponent* collider)
{
	// このコライダーを含む衝突ペアを全て削除（状態追跡の整合性維持）
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

	// コライダーリストから削除
	colliders_.erase(std::remove(colliders_.begin(), colliders_.end(), collider), colliders_.end());
}

/**
 * @brief 全コライダー間の衝突判定を実行
 */
void CollisionManager::CheckCollisions()
{
#ifdef USE_IMGUI
	// ============================================================================
	// ImGuiデバッグ表示
	// ============================================================================
	ImGui::Begin("CollisionManager Colliders");

	// 登録されているコライダーのリスト表示
	ImGui::SeparatorText("Colliders");
	if (ImGui::CollapsingHeader("List"))
	{
		for (size_t i = 0; i < colliders_.size(); ++i)
		{
			ICollisionComponent* collider = colliders_[i];
			if (collider && collider->GetOwner())
			{
				// コライダー情報を表示
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

	// 現在の衝突状態を表示
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

	// ============================================================================
	// 衝突判定処理
	// ============================================================================
	
	// 新しい衝突ペアを格納するセット
	std::unordered_set<CollisionPair, CollisionPairHash> newCollisions;

	// 全コライダーの組み合わせで判定（ブルートフォース）
	for (size_t i = 0; i < colliders_.size(); ++i)
	{
		for (size_t j = i + 1; j < colliders_.size(); ++j)
		{
			ICollisionComponent* a = colliders_[i];
			ICollisionComponent* b = colliders_[j];

			bool isHit = false;

			ColliderType typeA = a->GetColliderType();
			ColliderType typeB = b->GetColliderType();

			// ============================================================================
			// 3Dモードの判定
			// ============================================================================
			if (dimension_ == CollisionDimension::Mode3D)
			{
				// AABB同士の衝突判定
				if (typeA == ColliderType::AABB && typeB == ColliderType::AABB)
				{
					// サブステップ判定の使用チェック（高速移動体のすり抜け防止）
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
			// ============================================================================
			// 2Dモードの判定
			// ============================================================================
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

			// ============================================================================
			// 衝突状態の追跡とコールバック呼び出し
			// ============================================================================
			if (isHit)
			{
				CollisionPair pair{ a, b };
				newCollisions.insert(pair);
				
				// 衝突開始時（OnEnter）：前フレームで衝突していなかった場合
				if (!currentCollisions_.contains(pair))
				{
					a->CallOnEnter(b->GetOwner());
					b->CallOnEnter(a->GetOwner());
				}
				else
				{
					// 衝突継続中（OnStay）：前フレームも衝突していた場合
					a->CallOnStay(b->GetOwner());
					b->CallOnStay(a->GetOwner());
				}
			}
		}
	}

	// ============================================================================
	// 衝突終了時（OnExit）の処理
	// ============================================================================
	for (const auto& pair : currentCollisions_)
	{
		// 今フレームで衝突していないペアはOnExitを呼び出し
		if (!newCollisions.contains(pair))
		{
			pair.a->CallOnExit(pair.b->GetOwner());
			pair.b->CallOnExit(pair.a->GetOwner());
		}
	}

	// 衝突状態を更新（次フレームの判定用）
	currentCollisions_ = std::move(newCollisions);
}

/**
 * @brief 全コライダーの前フレーム位置を更新
 */
void CollisionManager::UpdatePreviousPositions()
{
	// 各コライダーの現在位置を前フレーム位置として保存
	for (auto& collider : colliders_)
	{
		collider->SetPreviousPosition(collider->GetOwner()->GetPosition());
	}
}

/**
 * @brief コライダータイプから文字列を取得
 */
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

/**
 * @brief 衝突をログに出力（デバッグ用）
 */
void CollisionManager::LogCollision(const std::string& phase, const ICollisionComponent* a, const ICollisionComponent* b)
{
#ifdef _DEBUG
	// 衝突したオブジェクトの情報を取得
	std::string tagA = a->GetOwner()->GetTag();
	std::string tagB = b->GetOwner()->GetTag();
	std::string typeAString = GetColliderTypeString(a->GetColliderType());
	std::string typeBString = GetColliderTypeString(b->GetColliderType());

	// 衝突フェーズに応じた矢印記号
	Logger::Log("| Collision " + phase + " " +
				(phase == "Exit" ? "<-" : (phase == "Enter" ? "->" : "=="))
				+ " | " + tagA + ": " + typeAString + ", " + tagB + ": " + typeBString + "\n");
#endif
}
