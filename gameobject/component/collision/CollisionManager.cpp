#include "CollisionManager.h"
#include <algorithm>
#include <unordered_set>
#include <limits>

#include "math/AABB.h"
#include "engine/gameobject/component/collision/AABBCollider.h"
#include "engine/gameobject/component/collision/OBBCollider.h"
#include "engine/gameobject/component/collision/SphereCollider.h"
#include "engine/gameobject/component/collision/RayCollider.h"
#include "engine/gameobject/component/base/Collider.h"
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
	collisionMatrix_[static_cast<int>(ColliderType::AABB)][static_cast<int>(ColliderType::AABB)] = [](const Collider* a, const Collider* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckAABBvsAABBMTV(static_cast<const AABBCollider*>(a)->GetAABB(), static_cast<const AABBCollider*>(b)->GetAABB(), tmpMtv)) return false;
		outMtv = -tmpMtv;
		return true;
	};
	// OBB vs OBB
	collisionMatrix_[static_cast<int>(ColliderType::OBB)][static_cast<int>(ColliderType::OBB)] = [](const Collider* a, const Collider* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckOBBvsOBBMTV(static_cast<const OBBCollider*>(a)->GetOBB(), static_cast<const OBBCollider*>(b)->GetOBB(), tmpMtv)) return false;
		outMtv = -tmpMtv;
		return true;
	};
	// Sphere vs Sphere
	collisionMatrix_[static_cast<int>(ColliderType::Sphere)][static_cast<int>(ColliderType::Sphere)] = [](const Collider* a, const Collider* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckSpherevsSphereMTV(static_cast<const SphereCollider*>(a)->GetSphere(), static_cast<const SphereCollider*>(b)->GetSphere(), tmpMtv)) return false;
		outMtv = -tmpMtv;
		return true;
	};
	// Sphere vs AABB
	collisionMatrix_[static_cast<int>(ColliderType::Sphere)][static_cast<int>(ColliderType::AABB)] = [](const Collider* a, const Collider* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckSpherevsAABBMTV(static_cast<const SphereCollider*>(a)->GetSphere(), static_cast<const AABBCollider*>(b)->GetAABB(), tmpMtv)) return false;
		outMtv = -tmpMtv;
		return true;
	};
	collisionMatrix_[static_cast<int>(ColliderType::AABB)][static_cast<int>(ColliderType::Sphere)] = [](const Collider* a, const Collider* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckSpherevsAABBMTV(static_cast<const SphereCollider*>(b)->GetSphere(), static_cast<const AABBCollider*>(a)->GetAABB(), tmpMtv)) return false;
		outMtv = tmpMtv;
		return true;
	};
	// Sphere vs OBB
	collisionMatrix_[static_cast<int>(ColliderType::Sphere)][static_cast<int>(ColliderType::OBB)] = [](const Collider* a, const Collider* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckSpherevsOBBMTV(static_cast<const SphereCollider*>(a)->GetSphere(), static_cast<const OBBCollider*>(b)->GetOBB(), tmpMtv)) return false;
		outMtv = -tmpMtv;
		return true;
	};
	collisionMatrix_[static_cast<int>(ColliderType::OBB)][static_cast<int>(ColliderType::Sphere)] = [](const Collider* a, const Collider* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckSpherevsOBBMTV(static_cast<const SphereCollider*>(b)->GetSphere(), static_cast<const OBBCollider*>(a)->GetOBB(), tmpMtv)) return false;
		outMtv = tmpMtv;
		return true;
	};
	// AABB vs OBB
	collisionMatrix_[static_cast<int>(ColliderType::AABB)][static_cast<int>(ColliderType::OBB)] = [](const Collider* a, const Collider* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckAABBvsOBBMTV(static_cast<const AABBCollider*>(a)->GetAABB(), static_cast<const OBBCollider*>(b)->GetOBB(), tmpMtv)) return false;
		outMtv = -tmpMtv;
		return true;
	};
	collisionMatrix_[static_cast<int>(ColliderType::OBB)][static_cast<int>(ColliderType::AABB)] = [](const Collider* a, const Collider* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckAABBvsOBBMTV(static_cast<const AABBCollider*>(b)->GetAABB(), static_cast<const OBBCollider*>(a)->GetOBB(), tmpMtv)) return false;
		outMtv = tmpMtv;
		return true;
	};
	const auto rayAabb = [](const Collider* ray, const Collider* box, Vector3&) {
		return collisionAlgorithm::CheckRayvsAABB3D(static_cast<const RayCollider*>(ray), static_cast<const AABBCollider*>(box));
	};
	const auto rayObb = [](const Collider* ray, const Collider* box, Vector3&) {
		return collisionAlgorithm::CheckRayvsOBB3D(static_cast<const RayCollider*>(ray), static_cast<const OBBCollider*>(box));
	};
	const auto raySphere = [](const Collider* ray, const Collider* sphere, Vector3&) {
		return collisionAlgorithm::CheckRayvsSphere3D(static_cast<const RayCollider*>(ray), static_cast<const SphereCollider*>(sphere));
	};
	collisionMatrix_[static_cast<int>(ColliderType::Ray)][static_cast<int>(ColliderType::AABB)] = rayAabb;
	collisionMatrix_[static_cast<int>(ColliderType::Ray)][static_cast<int>(ColliderType::OBB)] = rayObb;
	collisionMatrix_[static_cast<int>(ColliderType::Ray)][static_cast<int>(ColliderType::Sphere)] = raySphere;
	collisionMatrix_[static_cast<int>(ColliderType::AABB)][static_cast<int>(ColliderType::Ray)] = [](const Collider* a, const Collider* b, Vector3&) { return collisionAlgorithm::CheckRayvsAABB3D(static_cast<const RayCollider*>(b), static_cast<const AABBCollider*>(a)); };
	collisionMatrix_[static_cast<int>(ColliderType::OBB)][static_cast<int>(ColliderType::Ray)] = [](const Collider* a, const Collider* b, Vector3&) { return collisionAlgorithm::CheckRayvsOBB3D(static_cast<const RayCollider*>(b), static_cast<const OBBCollider*>(a)); };
	collisionMatrix_[static_cast<int>(ColliderType::Sphere)][static_cast<int>(ColliderType::Ray)] = [](const Collider* a, const Collider* b, Vector3&) { return collisionAlgorithm::CheckRayvsSphere3D(static_cast<const RayCollider*>(b), static_cast<const SphereCollider*>(a)); };

	// --- 2. CCD (サブステップ) 判定マトリクス (ccdMatrix_) の登録 ---
	// AABB vs AABB
	ccdMatrix_[static_cast<int>(ColliderType::AABB)][static_cast<int>(ColliderType::AABB)] = [](const Collider* a, const Collider* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckAABBvsAABBSubstepMTV(
			static_cast<const AABBCollider*>(a)->GetAABB(), a->GetPreviousPosition(),
			static_cast<const AABBCollider*>(b)->GetAABB(), b->GetPreviousPosition(),
			tmpMtv)) return false;
		outMtv = -tmpMtv;
		return true;
	};
	// OBB vs OBB
	ccdMatrix_[static_cast<int>(ColliderType::OBB)][static_cast<int>(ColliderType::OBB)] = [](const Collider* a, const Collider* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckOBBvsOBBSubstepMTV(
			static_cast<const OBBCollider*>(a)->GetOBB(), a->GetPreviousPosition(),
			static_cast<const OBBCollider*>(b)->GetOBB(), b->GetPreviousPosition(),
			tmpMtv)) return false;
		outMtv = -tmpMtv;
		return true;
	};
	// Sphere vs Sphere
	ccdMatrix_[static_cast<int>(ColliderType::Sphere)][static_cast<int>(ColliderType::Sphere)] = [](const Collider* a, const Collider* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckSpherevsSphereSubstepMTV(static_cast<const SphereCollider*>(a)->GetSphere(), a->GetPreviousPosition(), static_cast<const SphereCollider*>(b)->GetSphere(), b->GetPreviousPosition(), tmpMtv)) return false;
		outMtv = -tmpMtv;
		return true;
	};
	// Sphere vs AABB
	ccdMatrix_[static_cast<int>(ColliderType::Sphere)][static_cast<int>(ColliderType::AABB)] = [](const Collider* a, const Collider* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckSpherevsAABBSubstepMTV(
			static_cast<const SphereCollider*>(a)->GetSphere(), a->GetPreviousPosition(),
			static_cast<const AABBCollider*>(b)->GetAABB(), b->GetPreviousPosition(),
			tmpMtv)) return false;
		outMtv = -tmpMtv;
		return true;
	};
	ccdMatrix_[static_cast<int>(ColliderType::AABB)][static_cast<int>(ColliderType::Sphere)] = [](const Collider* a, const Collider* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckSpherevsAABBSubstepMTV(
			static_cast<const SphereCollider*>(b)->GetSphere(), b->GetPreviousPosition(),
			static_cast<const AABBCollider*>(a)->GetAABB(), a->GetPreviousPosition(),
			tmpMtv)) return false;
		outMtv = tmpMtv;
		return true;
	};
	// Sphere vs OBB
	ccdMatrix_[static_cast<int>(ColliderType::Sphere)][static_cast<int>(ColliderType::OBB)] = [](const Collider* a, const Collider* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckSpherevsOBBSubstepMTV(
			static_cast<const SphereCollider*>(a)->GetSphere(), a->GetPreviousPosition(),
			static_cast<const OBBCollider*>(b)->GetOBB(), b->GetPreviousPosition(),
			tmpMtv)) return false;
		outMtv = -tmpMtv;
		return true;
	};
	ccdMatrix_[static_cast<int>(ColliderType::OBB)][static_cast<int>(ColliderType::Sphere)] = [](const Collider* a, const Collider* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckSpherevsOBBSubstepMTV(
			static_cast<const SphereCollider*>(b)->GetSphere(), b->GetPreviousPosition(),
			static_cast<const OBBCollider*>(a)->GetOBB(), a->GetPreviousPosition(),
			tmpMtv)) return false;
		outMtv = tmpMtv;
		return true;
	};
	// AABB vs OBB
	ccdMatrix_[static_cast<int>(ColliderType::AABB)][static_cast<int>(ColliderType::OBB)] = [](const Collider* a, const Collider* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckAABBvsOBBSubstepMTV(
			static_cast<const AABBCollider*>(a)->GetAABB(), a->GetPreviousPosition(),
			static_cast<const OBBCollider*>(b)->GetOBB(), b->GetPreviousPosition(),
			tmpMtv)) return false;
		outMtv = -tmpMtv;
		return true;
	};
	ccdMatrix_[static_cast<int>(ColliderType::OBB)][static_cast<int>(ColliderType::AABB)] = [](const Collider* a, const Collider* b, Vector3& outMtv) {
		Vector3 tmpMtv = {};
		if (!collisionAlgorithm::CheckAABBvsOBBSubstepMTV(
			static_cast<const AABBCollider*>(b)->GetAABB(), b->GetPreviousPosition(),
			static_cast<const OBBCollider*>(a)->GetOBB(), a->GetPreviousPosition(),
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
	currentObjectCollisions_.clear();
	nextObjectCollisions_.clear();
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

void CollisionManager::Register(Collider* collider)
{
	if (collider && std::find(colliders_.begin(), colliders_.end(), collider) == colliders_.end()) colliders_.push_back(collider);
	raycastCandidates_.reserve(colliders_.size());
}

void CollisionManager::Unregister(Collider* collider)
{
	GameObject* owner = collider ? collider->GetOwner() : nullptr;
	if (!collider || !owner)
	{
		return;
	}

	// 外した判定の Exit はここで送る。次のフレームまで残すと相手が片方だけ通知を受け損ねる。
	for (auto it = currentCollisions_.begin(); it != currentCollisions_.end(); )
	{
		if (it->a == collider || it->b == collider)
		{
			const Collider* a = it->a;
			const Collider* b = it->b;
			CollisionInfo infoA{ const_cast<Collider*>(a), b->GetOwner(), const_cast<Collider*>(b) };
			CollisionInfo infoB{ const_cast<Collider*>(b), a->GetOwner(), const_cast<Collider*>(a) };
			if (!a->GetOwner()->IsDestroying()) a->CallOnExit(infoA);
			if (!b->GetOwner()->IsDestroying()) b->CallOnExit(infoB);
			it = currentCollisions_.erase(it);
		}
		else
		{
			++it;
		}
	}

	colliders_.erase(std::remove(colliders_.begin(), colliders_.end(), collider), colliders_.end());
	for (auto it = currentObjectCollisions_.begin(); it != currentObjectCollisions_.end(); )
	{
		if (it->a != owner && it->b != owner)
		{
			++it;
			continue;
		}

		const ObjectPair objects = *it;
		const bool hasRemainingContact = std::any_of(currentCollisions_.begin(), currentCollisions_.end(), [&objects](const CollisionPair& pair)
		{
			GameObject* a = pair.a->GetOwner();
			GameObject* b = pair.b->GetOwner();
			return (a == objects.a && b == objects.b) || (a == objects.b && b == objects.a);
		});
		if (hasRemainingContact)
		{
			++it;
			continue;
		}

		CollisionInfo infoA{ nullptr, objects.b, nullptr };
		CollisionInfo infoB{ nullptr, objects.a, nullptr };
		if (!objects.a->IsDestroying()) objects.a->DispatchObjectCollisionExit(infoA);
		if (!objects.b->IsDestroying()) objects.b->DispatchObjectCollisionExit(infoB);
		it = currentObjectCollisions_.erase(it);
	}
}

void CollisionManager::CheckCollisions()
{
	nextCollisions_.clear();
	collisionDetails_.clear();
	auto& newCollisions = nextCollisions_;
	auto& detailsMap = collisionDetails_;

	// --- 1. ブロードフェーズ: 各コライダーをグリッドセルに登録 ---
	for (auto& [key, bucket] : gridBuckets_) bucket.clear();
	for (auto& collider : colliders_)
	{
		// GameObject自身が非アクティブ、またはコライダー個別で非アクティブな場合は判定しない
		if (!collider->GetOwner()->IsActive() || !collider->IsActive())
		{
			continue;
		}

		AABB broadphaseAABB = collider->GetBroadphaseAABB();
		if (collider->UseSubstep())
		{
			const Vector3 currentPosition = MathUtils::GetTranslateFromMatrix(collider->GetOwner()->GetWorldMatrix());
			const Vector3 offset = collider->GetPreviousPosition() - currentPosition;
			broadphaseAABB.min_.x = (std::min)(broadphaseAABB.min_.x, broadphaseAABB.min_.x + offset.x);
			broadphaseAABB.min_.y = (std::min)(broadphaseAABB.min_.y, broadphaseAABB.min_.y + offset.y);
			broadphaseAABB.min_.z = (std::min)(broadphaseAABB.min_.z, broadphaseAABB.min_.z + offset.z);
			broadphaseAABB.max_.x = (std::max)(broadphaseAABB.max_.x, broadphaseAABB.max_.x + offset.x);
			broadphaseAABB.max_.y = (std::max)(broadphaseAABB.max_.y, broadphaseAABB.max_.y + offset.y);
			broadphaseAABB.max_.z = (std::max)(broadphaseAABB.max_.z, broadphaseAABB.max_.z + offset.z);
		}

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
				Collider* a = bucket[i];
				Collider* b = bucket[j];
				if (a->GetOwner() == b->GetOwner())
				{
					continue;
				}

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
					else if (collisionMatrix_[typeA][typeB])
					{
						isHit = collisionMatrix_[typeA][typeB](pair.a, pair.b, mtv);
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
		infoA.self = const_cast<Collider*>(pair.a);
		infoA.other = pair.b->GetOwner();
		infoA.otherCollider = const_cast<Collider*>(pair.b);
		infoA.normal = details.normal; // a から見た押し出し方向
		infoA.depth = details.depth;
		infoA.collisionPoint = details.point;

		// b 側の衝突情報
		CollisionInfo infoB;
		infoB.self = const_cast<Collider*>(pair.b);
		infoB.other = pair.a->GetOwner();
		infoB.otherCollider = const_cast<Collider*>(pair.a);
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
			infoA.self = const_cast<Collider*>(pair.a);
			infoA.other = pair.b->GetOwner();
			infoA.otherCollider = const_cast<Collider*>(pair.b);

			CollisionInfo infoB;
			infoB.self = const_cast<Collider*>(pair.b);
			infoB.other = pair.a->GetOwner();
			infoB.otherCollider = const_cast<Collider*>(pair.a);

			pair.a->CallOnExit(infoA);
			pair.b->CallOnExit(infoB);
			LogCollision("Exit", pair.a, pair.b);
		}
	}

	nextObjectCollisions_.clear();
	for (const auto& collision : newCollisions)
	{
		ObjectPair objects{ collision.a->GetOwner(), collision.b->GetOwner() };
		if (objects.a > objects.b) std::swap(objects.a, objects.b);
		if (objects.a != objects.b) nextObjectCollisions_.insert(objects);
	}
	for (const auto& objects : nextObjectCollisions_)
	{
		const auto representative = std::find_if(newCollisions.begin(), newCollisions.end(), [&objects](const CollisionPair& pair)
		{
			return (pair.a->GetOwner() == objects.a && pair.b->GetOwner() == objects.b) || (pair.a->GetOwner() == objects.b && pair.b->GetOwner() == objects.a);
		});
		if (representative == newCollisions.end()) continue;
		const Collider* colliderA = representative->a->GetOwner() == objects.a ? representative->a : representative->b;
		const Collider* colliderB = colliderA == representative->a ? representative->b : representative->a;
		CollisionInfo infoA{ const_cast<Collider*>(colliderA), objects.b, const_cast<Collider*>(colliderB) };
		CollisionInfo infoB{ const_cast<Collider*>(colliderB), objects.a, const_cast<Collider*>(colliderA) };
		if (currentObjectCollisions_.contains(objects))
		{
			objects.a->DispatchObjectCollisionStay(infoA);
			objects.b->DispatchObjectCollisionStay(infoB);
		}
		else
		{
			objects.a->DispatchObjectCollisionEnter(infoA);
			objects.b->DispatchObjectCollisionEnter(infoB);
		}
	}
	for (const auto& objects : currentObjectCollisions_)
	{
		if (!nextObjectCollisions_.contains(objects))
		{
			CollisionInfo infoA{};
			infoA.other = objects.b;
			CollisionInfo infoB{};
			infoB.other = objects.a;
			objects.a->DispatchObjectCollisionExit(infoA);
			objects.b->DispatchObjectCollisionExit(infoB);
		}
	}
	currentObjectCollisions_.swap(nextObjectCollisions_);
	currentCollisions_.swap(nextCollisions_);
}

void CollisionManager::UpdatePreviousPositions()
{
	for (auto& collider : colliders_)
	{
		// トンネリング(CCD)判定を正しく行うため、ワールド座標で保持する
		collider->SetPreviousPosition(MathUtils::GetTranslateFromMatrix(collider->GetOwner()->GetWorldMatrix()));
	}
}

bool CollisionManager::Raycast(const Ray& ray, uint32_t mask, RaycastHit& outHit) const
{
	raycastCandidates_.clear();
	const Vector3 end = ray.start + ray.direction * ray.length;
	const Vector3 minimum{ (std::min)(ray.start.x, end.x), (std::min)(ray.start.y, end.y), (std::min)(ray.start.z, end.z) };
	const Vector3 maximum{ (std::max)(ray.start.x, end.x), (std::max)(ray.start.y, end.y), (std::max)(ray.start.z, end.z) };
	for (int x = static_cast<int>(std::floor(minimum.x / cellSize_)); x <= static_cast<int>(std::floor(maximum.x / cellSize_)); ++x)
	{
		for (int y = static_cast<int>(std::floor(minimum.y / cellSize_)); y <= static_cast<int>(std::floor(maximum.y / cellSize_)); ++y)
		{
			for (int z = static_cast<int>(std::floor(minimum.z / cellSize_)); z <= static_cast<int>(std::floor(maximum.z / cellSize_)); ++z)
			{
				auto bucket = gridBuckets_.find({ x, y, z });
				if (bucket == gridBuckets_.end()) continue;
				for (auto* collider : bucket->second)
				{
					if (std::find(raycastCandidates_.begin(), raycastCandidates_.end(), collider) == raycastCandidates_.end()) raycastCandidates_.push_back(collider);
				}
			}
		}
	}
	const auto& candidates = gridBuckets_.empty() ? colliders_ : raycastCandidates_;
	float nearest = (std::numeric_limits<float>::max)();
	GameObjectComponent::Collider* nearestCollider = nullptr;
	for (auto* collider : candidates)
	{
		if (!collider || !collider->IsActive() || collider->GetColliderType() == ColliderType::Ray || !(mask & static_cast<uint32_t>(collider->GetCollisionLayer()))) continue;
		float distance = 0.0f;
		bool hit = false;
		switch (collider->GetColliderType())
		{
		case ColliderType::AABB: hit = collisionAlgorithm::CheckRayvsAABB(ray, static_cast<AABBCollider*>(collider)->GetAABB(), &distance); break;
		case ColliderType::OBB: hit = collisionAlgorithm::CheckRayvsOBB(ray, static_cast<OBBCollider*>(collider)->GetOBB(), &distance); break;
		case ColliderType::Sphere: hit = collisionAlgorithm::CheckRayvsSphere(ray, static_cast<SphereCollider*>(collider)->GetSphere(), &distance); break;
		default: break;
		}
		if (hit && distance < nearest)
		{
			nearest = distance;
			nearestCollider = collider;
		}
	}
	if (!nearestCollider)
	{
		outHit = {};
		return false;
	}
	outHit.object = nearestCollider->GetOwner();
	outHit.collider = nearestCollider;
	outHit.distance = nearest;
	outHit.position = ray.start + ray.direction * nearest;
	return true;
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

void CollisionManager::LogCollision(const std::string& phase, const Collider* a, const Collider* b)
{
#ifdef _DEBUG
	std::string tagA = a->GetOwner()->GetTag();
	std::string tagB = b->GetOwner()->GetTag();
	std::string typeAString = GetColliderTypeString(a->GetColliderType());
	std::string typeBString = GetColliderTypeString(b->GetColliderType());

	KCE::Logger::Log("| Collision " + phase + " " +
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
			Collider* collider = colliders_[i];
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
