#pragma once
#include <unordered_set>

#include "AABBColliderComponent.h"
#include "CollisionAlgorithm.h"
#include "OBBColliderComponent.h"
#include "application/GameObject/component/base/ICollisionComponent.h"

enum class CollisionDimension
{
	Mode2D,
	Mode3D
};

class CollisionManager
{
public:
	static CollisionManager* GetInstance();
	void Initialize() { colliders_.clear(); }
	void Finalize() { colliders_.clear(), currentCollisions_.clear(); }

	void Register(ICollisionComponent* collider);
	void Unregister(ICollisionComponent* collider);
	void CheckCollisions();
	void UpdatePreviousPositions();

	// 衝突判定の次元を設定
	void SetCollisionDimension(CollisionDimension dimension) { dimension_ = dimension; }

	// 2Dモード時の衝突判定面を設定
	void SetCollisionPlane(CollisionPlane plane) { collisionPlane_ = plane; }

private:
	static CollisionManager* instance_; // シングルトンインスタンス
	CollisionManager() = default;
	~CollisionManager() = default;
	CollisionManager(const CollisionManager&) = delete;
	CollisionManager& operator=(const CollisionManager&) = delete;

	// 衝突判定関数
	bool CheckCollision(const AABBColliderComponent* a, const AABBColliderComponent* b);			// AABB同士の衝突判定
	bool CheckCollision(const OBBColliderComponent* a, const OBBColliderComponent* b);				// OBB同士の衝突判定
	bool CheckCollision(const AABBColliderComponent* a, const OBBColliderComponent* b);				// AABBとOBBの衝突判定

	// 衝突判定関数（サブステップ）
	bool CheckSubstepCollision(const AABBColliderComponent* a, const AABBColliderComponent* b);
	bool CheckSubstepCollision(const OBBColliderComponent* a, const OBBColliderComponent* b);
	bool CheckSubstepCollision(const AABBColliderComponent* a, const OBBColliderComponent* b);

	//コライダータイプから文字列を取得
	std::string GetColliderTypeString(ColliderType type) const;
	//　衝突したらログを出力
	void LogCollision(const std::string& phase, const ICollisionComponent* a, const ICollisionComponent* b);

	// ペアを識別するためのキー
	struct CollisionPair
	{
		const ICollisionComponent* a;
		const ICollisionComponent* b;

		bool operator==(const CollisionPair& other) const
		{
			return (a == other.a && b == other.b) || (a == other.b && b == other.a);
		}
	};

	struct CollisionPairHash
	{
		std::size_t operator()(const CollisionPair& pair) const
		{
			return std::hash<const void*>()(pair.a) ^ std::hash<const void*>()(pair.b);
		}
	};

	std::vector<ICollisionComponent*> colliders_;

	// 現在接触しているペア
	std::unordered_set<CollisionPair, CollisionPairHash> currentCollisions_;

	// 衝突判定の次元
	CollisionDimension dimension_ = CollisionDimension::Mode3D;

	// 2Dモード時の衝突判定面
	CollisionPlane collisionPlane_ = CollisionPlane::XY;
};

