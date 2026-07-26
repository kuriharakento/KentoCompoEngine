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
#include "manager/editor/DebugUIManager.h"
#include "math/MathUtils.h"
#include "engine/gameobject/component/collision/CollisionAlgorithm.h"

namespace KCE
{
using namespace GameObjectComponent;

std::unique_ptr<CollisionManager> CollisionManager::instance_ = nullptr;

CollisionManager* CollisionManager::GetInstance()
{
	if (instance_ == nullptr)
	{
		instance_ = std::make_unique<CollisionManager>();
	}
	return instance_.get();
}
void CollisionManager::Initialize()
{
	colliders_.clear();
#ifdef USE_IMGUI
	DebugUIManager::GetInstance()->RegisterDebugUI(this, "CollisionManager Colliders", [this]() { this->DrawImGui(); }, DebugUIArea::Console);
#endif

	// --- 1. 通常判定マトリクス (collisionMatrix_) の登録 ---
	// AABB vs AABB
	collisionMatrix_[static_cast<int>(ColliderType::AABB)][static_cast<int>(ColliderType::AABB)] = [](const ICollisionComponent* a, const ICollisionComponent* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckAABBvsAABBMTV(static_cast<const AABBColliderComponent*>(a)->GetAABB(), static_cast<const AABBColliderComponent*>(b)->GetAABB(), tmpMtv)) return false;
		outMtv = -tmpMtv;
		return true;
	};
	// OBB vs OBB
	collisionMatrix_[static_cast<int>(ColliderType::OBB)][static_cast<int>(ColliderType::OBB)] = [](const ICollisionComponent* a, const ICollisionComponent* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckOBBvsOBBMTV(static_cast<const OBBColliderComponent*>(a)->GetOBB(), static_cast<const OBBColliderComponent*>(b)->GetOBB(), tmpMtv)) return false;
		outMtv = -tmpMtv;
		return true;
	};
	// Sphere vs Sphere
	collisionMatrix_[static_cast<int>(ColliderType::Sphere)][static_cast<int>(ColliderType::Sphere)] = [](const ICollisionComponent* a, const ICollisionComponent* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckSpherevsSphereMTV(static_cast<const SphereColliderComponent*>(a)->GetSphere(), static_cast<const SphereColliderComponent*>(b)->GetSphere(), tmpMtv)) return false;
		outMtv = -tmpMtv;
		return true;
	};
	// Sphere vs AABB
	collisionMatrix_[static_cast<int>(ColliderType::Sphere)][static_cast<int>(ColliderType::AABB)] = [](const ICollisionComponent* a, const ICollisionComponent* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckSpherevsAABBMTV(static_cast<const SphereColliderComponent*>(a)->GetSphere(), static_cast<const AABBColliderComponent*>(b)->GetAABB(), tmpMtv)) return false;
		outMtv = -tmpMtv;
		return true;
	};
	collisionMatrix_[static_cast<int>(ColliderType::AABB)][static_cast<int>(ColliderType::Sphere)] = [](const ICollisionComponent* a, const ICollisionComponent* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckSpherevsAABBMTV(static_cast<const SphereColliderComponent*>(b)->GetSphere(), static_cast<const AABBColliderComponent*>(a)->GetAABB(), tmpMtv)) return false;
		outMtv = tmpMtv;
		return true;
	};
	// Sphere vs OBB
	collisionMatrix_[static_cast<int>(ColliderType::Sphere)][static_cast<int>(ColliderType::OBB)] = [](const ICollisionComponent* a, const ICollisionComponent* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckSpherevsOBBMTV(static_cast<const SphereColliderComponent*>(a)->GetSphere(), static_cast<const OBBColliderComponent*>(b)->GetOBB(), tmpMtv)) return false;
		outMtv = -tmpMtv;
		return true;
	};
	collisionMatrix_[static_cast<int>(ColliderType::OBB)][static_cast<int>(ColliderType::Sphere)] = [](const ICollisionComponent* a, const ICollisionComponent* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckSpherevsOBBMTV(static_cast<const SphereColliderComponent*>(b)->GetSphere(), static_cast<const OBBColliderComponent*>(a)->GetOBB(), tmpMtv)) return false;
		outMtv = tmpMtv;
		return true;
	};
	// AABB vs OBB
	collisionMatrix_[static_cast<int>(ColliderType::AABB)][static_cast<int>(ColliderType::OBB)] = [](const ICollisionComponent* a, const ICollisionComponent* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckAABBvsOBBMTV(static_cast<const AABBColliderComponent*>(a)->GetAABB(), static_cast<const OBBColliderComponent*>(b)->GetOBB(), tmpMtv)) return false;
		outMtv = -tmpMtv;
		return true;
	};
	collisionMatrix_[static_cast<int>(ColliderType::OBB)][static_cast<int>(ColliderType::AABB)] = [](const ICollisionComponent* a, const ICollisionComponent* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckAABBvsOBBMTV(static_cast<const AABBColliderComponent*>(b)->GetAABB(), static_cast<const OBBColliderComponent*>(a)->GetOBB(), tmpMtv)) return false;
		outMtv = tmpMtv;
		return true;
	};

	// --- 2. CCD (サブステップ) 判定マトリクス (ccdMatrix_) の登録 ---
	// AABB vs AABB
	ccdMatrix_[static_cast<int>(ColliderType::AABB)][static_cast<int>(ColliderType::AABB)] = [](const ICollisionComponent* a, const ICollisionComponent* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckAABBvsAABBSubstepMTV(
			static_cast<const AABBColliderComponent*>(a)->GetAABB(), a->GetPreviousPosition(),
			static_cast<const AABBColliderComponent*>(b)->GetAABB(), b->GetPreviousPosition(),
			tmpMtv)) return false;
		outMtv = -tmpMtv;
		return true;
	};
	// OBB vs OBB
	ccdMatrix_[static_cast<int>(ColliderType::OBB)][static_cast<int>(ColliderType::OBB)] = [](const ICollisionComponent* a, const ICollisionComponent* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckOBBvsOBBSubstepMTV(
			static_cast<const OBBColliderComponent*>(a)->GetOBB(), a->GetPreviousPosition(),
			static_cast<const OBBColliderComponent*>(b)->GetOBB(), b->GetPreviousPosition(),
			tmpMtv)) return false;
		outMtv = -tmpMtv;
		return true;
	};
	// Sphere vs Sphere
	ccdMatrix_[static_cast<int>(ColliderType::Sphere)][static_cast<int>(ColliderType::Sphere)] = [](const ICollisionComponent* a, const ICollisionComponent* b, Vector3& outMtv) {
		if (!collisionAlgorithm::CheckSpherevsSphereSubstep3D(static_cast<const SphereColliderComponent*>(a), static_cast<const SphereColliderComponent*>(b))) return false;
		Vector3 tmpMtv = {};
		collisionAlgorithm::CheckSpherevsSphereMTV(static_cast<const SphereColliderComponent*>(a)->GetSphere(), static_cast<const SphereColliderComponent*>(b)->GetSphere(), tmpMtv);
		outMtv = -tmpMtv;
		return true;
	};
	// Sphere vs AABB
	ccdMatrix_[static_cast<int>(ColliderType::Sphere)][static_cast<int>(ColliderType::AABB)] = [](const ICollisionComponent* a, const ICollisionComponent* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckSpherevsAABBSubstepMTV(
			static_cast<const SphereColliderComponent*>(a)->GetSphere(), a->GetPreviousPosition(),
			static_cast<const AABBColliderComponent*>(b)->GetAABB(), b->GetPreviousPosition(),
			tmpMtv)) return false;
		outMtv = -tmpMtv;
		return true;
	};
	ccdMatrix_[static_cast<int>(ColliderType::AABB)][static_cast<int>(ColliderType::Sphere)] = [](const ICollisionComponent* a, const ICollisionComponent* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckSpherevsAABBSubstepMTV(
			static_cast<const SphereColliderComponent*>(b)->GetSphere(), b->GetPreviousPosition(),
			static_cast<const AABBColliderComponent*>(a)->GetAABB(), a->GetPreviousPosition(),
			tmpMtv)) return false;
		outMtv = tmpMtv;
		return true;
	};
	// Sphere vs OBB
	ccdMatrix_[static_cast<int>(ColliderType::Sphere)][static_cast<int>(ColliderType::OBB)] = [](const ICollisionComponent* a, const ICollisionComponent* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckSpherevsOBBSubstepMTV(
			static_cast<const SphereColliderComponent*>(a)->GetSphere(), a->GetPreviousPosition(),
			static_cast<const OBBColliderComponent*>(b)->GetOBB(), b->GetPreviousPosition(),
			tmpMtv)) return false;
		outMtv = -tmpMtv;
		return true;
	};
	ccdMatrix_[static_cast<int>(ColliderType::OBB)][static_cast<int>(ColliderType::Sphere)] = [](const ICollisionComponent* a, const ICollisionComponent* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckSpherevsOBBSubstepMTV(
			static_cast<const SphereColliderComponent*>(b)->GetSphere(), b->GetPreviousPosition(),
			static_cast<const OBBColliderComponent*>(a)->GetOBB(), a->GetPreviousPosition(),
			tmpMtv)) return false;
		outMtv = tmpMtv;
		return true;
	};
	// AABB vs OBB
	ccdMatrix_[static_cast<int>(ColliderType::AABB)][static_cast<int>(ColliderType::OBB)] = [](const ICollisionComponent* a, const ICollisionComponent* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckAABBvsOBBSubstepMTV(
			static_cast<const AABBColliderComponent*>(a)->GetAABB(), a->GetPreviousPosition(),
			static_cast<const OBBColliderComponent*>(b)->GetOBB(), b->GetPreviousPosition(),
			tmpMtv)) return false;
		outMtv = -tmpMtv;
		return true;
	};
	ccdMatrix_[static_cast<int>(ColliderType::OBB)][static_cast<int>(ColliderType::AABB)] = [](const ICollisionComponent* a, const ICollisionComponent* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckAABBvsOBBSubstepMTV(
			static_cast<const AABBColliderComponent*>(b)->GetAABB(), b->GetPreviousPosition(),
			static_cast<const OBBColliderComponent*>(a)->GetOBB(), a->GetPreviousPosition(),
			tmpMtv)) return false;
		outMtv = tmpMtv;
		return true;
	};
}

void CollisionManager::Finalize()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->UnregisterDebugUI(this);
	}
#endif
	colliders_.clear();
	currentCollisions_.clear();
	instance_.reset();
}

CollisionManager::~CollisionManager()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->UnregisterDebugUI(this);
	}
#endif
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
	// 新しい衝突ペアを格納するセット
	std::unordered_set<CollisionPair, CollisionPairHash> newCollisions;

	// 衝突詳細情報を一時保存するマップ
	struct CollisionDetails
	{
		Vector3 normal; // a から見た押し出し方向
		float depth;
		Vector3 point;
	};
	std::unordered_map<CollisionPair, CollisionDetails, CollisionPairHash> detailsMap;

	// --- 1. ブロードフェーズ: 各コライダーをグリッドセルに登録 ---
	gridBuckets_.clear();
	for (auto& collider : colliders_)
	{
		// GameObject自身が非アクティブ、またはコライダー個別で非アクティブな場合は判定しない
		if (!collider->GetOwner()->IsActive() || !collider->IsActive())
		{
			continue;
		}

		AABB broadphaseAABB = collider->GetBroadphaseAABB();

		// 境界座標からセルインデックスの最小・最大を計算
		int minX = static_cast<int>(std::floor(broadphaseAABB.min_.x / cellSize_));
		int minY = static_cast<int>(std::floor(broadphaseAABB.min_.y / cellSize_));
		int minZ = static_cast<int>(std::floor(broadphaseAABB.min_.z / cellSize_));

		int maxX = static_cast<int>(std::floor(broadphaseAABB.max_.x / cellSize_));
		int maxY = static_cast<int>(std::floor(broadphaseAABB.max_.y / cellSize_));
		int maxZ = static_cast<int>(std::floor(broadphaseAABB.max_.z / cellSize_));

		// 重複してまたがっているすべてのセルに登録
		for (int x = minX; x <= maxX; ++x)
		{
			for (int y = minY; y <= maxY; ++y)
			{
				for (int z = minZ; z <= maxZ; ++z)
				{
					gridBuckets_[{x, y, z}].push_back(collider);
				}
			}
		}
	}

	// --- 2. ナローフェーズ: 同一セル内のコライダー同士を総当たり判定 ---
	for (auto& [key, bucket] : gridBuckets_)
	{
		if (bucket.size() < 2)
		{
			continue;
		}

		for (size_t i = 0; i < bucket.size(); ++i)
		{
			for (size_t j = i + 1; j < bucket.size(); ++j)
			{
				ICollisionComponent* a = bucket[i];
				ICollisionComponent* b = bucket[j];

				// 重複判定を防ぐためにポインタアドレス順でソートしたキーを使用
				CollisionPair pair = { a, b };
				if (pair.a > pair.b)
				{
					std::swap(pair.a, pair.b);
				}

				if (newCollisions.find(pair) != newCollisions.end())
				{
					continue; // 既に別のセルで判定・検知済み
				}

				// 衝突レイヤーによるフィルタリング
				if (!(a->GetCollisionMask() & static_cast<uint32_t>(b->GetCollisionLayer())) ||
					!(b->GetCollisionMask() & static_cast<uint32_t>(a->GetCollisionLayer())))
				{
					continue;
				}

				bool isHit = false;
				Vector3 mtv = {};

				int typeA = static_cast<int>(pair.a->GetColliderType());
				int typeB = static_cast<int>(pair.b->GetColliderType());

				if (pair.a->UseSubstep() || pair.b->UseSubstep())
				{
					if (ccdMatrix_[typeA][typeB])
					{
						isHit = ccdMatrix_[typeA][typeB](pair.a, pair.b, mtv);
					}
				}
				else
				{
					if (collisionMatrix_[typeA][typeB])
					{
						isHit = collisionMatrix_[typeA][typeB](pair.a, pair.b, mtv);
					}
				}

				if (isHit)
				{
					newCollisions.insert(pair);

					// MTVから法線とめり込み深さを算出
					Vector3 normal = {};
					float depth = 0.0f;
					float length = mtv.Length();
					if (length > 0.0001f)
					{
						normal = mtv / length;
						depth = length;
					}

					// 衝突点の計算
					Vector3 point = (a->GetCollisionPosition() + b->GetCollisionPosition()) * 0.5f;

					detailsMap[pair] = { normal, depth, point };
				}
			}
		}
	}

	// 衝突状態の変化を検出し、コールバックを呼び出す
	for (auto& pair : newCollisions)
	{
		auto detIt = detailsMap.find(pair);
		CollisionDetails details = {};
		if (detIt != detailsMap.end())
		{
			details = detIt->second;
		}

		// a 側の衝突情報
		CollisionInfo infoA;
		infoA.other = pair.b->GetOwner();
		infoA.otherCollider = const_cast<ICollisionComponent*>(pair.b);
		infoA.normal = details.normal; // a から見た押し出し方向
		infoA.depth = details.depth;
		infoA.collisionPoint = details.point;

		// b 側の衝突情報
		CollisionInfo infoB;
		infoB.other = pair.a->GetOwner();
		infoB.otherCollider = const_cast<ICollisionComponent*>(pair.a);
		infoB.normal = -details.normal; // b から見たら押し出し方向は逆向き
		infoB.depth = details.depth;
		infoB.collisionPoint = details.point;

		if (currentCollisions_.find(pair) == currentCollisions_.end())
		{
			// 新規衝突 (OnEnter)
			pair.a->CallOnEnter(infoA);
			pair.b->CallOnEnter(infoB);
			LogCollision("Enter", pair.a, pair.b);
		}
		else
		{
			// 継続衝突 (OnStay)
			pair.a->CallOnStay(infoA);
			pair.b->CallOnStay(infoB);
		}
	}

	for (auto& pair : currentCollisions_)
	{
		if (newCollisions.find(pair) == newCollisions.end())
		{
			// 衝突終了 (OnExit)
			// Exitのときはめり込みは無いため深さ0、法線0にする
			CollisionInfo infoA;
			infoA.other = pair.b->GetOwner();
			infoA.otherCollider = const_cast<ICollisionComponent*>(pair.b);
			
			CollisionInfo infoB;
			infoB.other = pair.a->GetOwner();
			infoB.otherCollider = const_cast<ICollisionComponent*>(pair.a);

			pair.a->CallOnExit(infoA);
			pair.b->CallOnExit(infoB);
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

#ifdef USE_IMGUI
void CollisionManager::DrawImGui()
{
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
}
#endif
} // namespace KCE
