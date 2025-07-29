#include "CollisionManager.h"
#include <algorithm>

#include "math/AABB.h"
#include "application/GameObject/component/collision/AABBColliderComponent.h"
#include "application/GameObject/component/base/ICollisionComponent.h"
#include "application/GameObject/base/GameObject.h"
#include "base/Logger.h"
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

			//コライダーのタイプごとに衝突判定を行う
			/* AABB vs AABB */
			if (typeA == ColliderType::AABB && typeB == ColliderType::AABB)
			{
				if (a->UseSweep() || b->UseSweep())
				{
					isHit = CheckSubstepCollision(static_cast<AABBColliderComponent*>(a), static_cast<AABBColliderComponent*>(b));
				}
				else
				{
					isHit = CheckCollision(static_cast<AABBColliderComponent*>(a), static_cast<AABBColliderComponent*>(b));
				}
			}
			/* OBB vs OBB */
			else if (typeA == ColliderType::OBB && typeB == ColliderType::OBB)
			{
				if (a->UseSweep() || b->UseSweep())
				{
					isHit = CheckSubstepCollision(static_cast<OBBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b));
				}
				else
				{
					isHit = CheckCollision(static_cast<OBBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b));
				}
			}
			/* AABB vs OBB */
			else if (typeA == ColliderType::AABB && typeB == ColliderType::OBB)
			{
				if (a->UseSweep() || b->UseSweep())
				{
					isHit = CheckSubstepCollision(static_cast<AABBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b));
				}
				else
				{
					isHit = CheckCollision(static_cast<AABBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b));
				}
			}
			else if (typeA == ColliderType::OBB && typeB == ColliderType::AABB)
			{
				if (a->UseSweep() || b->UseSweep())
				{
					isHit = CheckSubstepCollision(static_cast<AABBColliderComponent*>(b), static_cast<OBBColliderComponent*>(a));
				}
				else
				{
					isHit = CheckCollision(static_cast<AABBColliderComponent*>(b), static_cast<OBBColliderComponent*>(a));
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
					//ログで確認できるように表示
					//LogCollision("Enter", a, b);
				}
				else
				{
					//衝突している間の処理
					a->CallOnStay(b->GetOwner());
					b->CallOnStay(a->GetOwner());
					//ログで確認できるように表示
					//LogCollision("Stay", a, b);
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
			//ログで確認できるように表示
			//LogCollision("Exit", pair.a, pair.b);
		}
	}

	currentCollisions_ = std::move(newCollisions);

	//現在登録されている数をログに出力
	static float time = 0.0f;
	time += 1.0f / 60.0f; // 60FPS想定

	if (time >= 1.0f) // 1秒ごとにログ出力
	{
		Logger::Log("CollisionManager: Current colliders count: " + std::to_string(colliders_.size()) + "\n");
		time = 0.0f; // リセット
	}
}

void CollisionManager::UpdatePreviousPositions()
{
	for (auto& collider : colliders_)
	{
		if (collider)
		{
			collider->SetPreviousPosition(collider->GetOwner()->GetPosition());
		}
	}
}

bool CollisionManager::CheckCollision(const AABBColliderComponent* a, const AABBColliderComponent* b)
{
	const AABB& aBox = a->GetAABB();
	const AABB& bBox = b->GetAABB();

	return (aBox.max_.x >= bBox.min_.x && aBox.min_.x <= bBox.max_.x) &&
		(aBox.max_.y >= bBox.min_.y && aBox.min_.y <= bBox.max_.y) &&
		(aBox.max_.z >= bBox.min_.z && aBox.min_.z <= bBox.max_.z);
}

bool CollisionManager::CheckCollision(const OBBColliderComponent* a, const OBBColliderComponent* b)
{
	const OBB& obbA = a->GetOBB();
	const OBB& obbB = b->GetOBB();

	// OBBの回転行列（各軸ベクトル）
	Matrix4x4 rotA = obbA.rotate;
	Matrix4x4 rotB = obbB.rotate;

	Vector3 axesA[3] =
	{
		Vector3::Normalize(Vector3(rotA.m[0][0], rotA.m[0][1], rotA.m[0][2])),
		Vector3::Normalize(Vector3(rotA.m[1][0], rotA.m[1][1], rotA.m[1][2])),
		Vector3::Normalize(Vector3(rotA.m[2][0], rotA.m[2][1], rotA.m[2][2]))
	};

	Vector3 axesB[3] =
	{
		Vector3::Normalize(Vector3(rotB.m[0][0], rotB.m[0][1], rotB.m[0][2])),
		Vector3::Normalize(Vector3(rotB.m[1][0], rotB.m[1][1], rotB.m[1][2])),
		Vector3::Normalize(Vector3(rotB.m[2][0], rotB.m[2][1], rotB.m[2][2]))
	};

	// 15の分離軸
	Vector3 testAxes[15];
	int axisCount = 0;

	// OBB A, Bのローカル軸
	for (int i = 0; i < 3; ++i) testAxes[axisCount++] = axesA[i];
	for (int i = 0; i < 3; ++i) testAxes[axisCount++] = axesB[i];

	// クロス積
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			testAxes[axisCount++] = Vector3::Normalize(Vector3::Cross(axesA[i], axesB[j]));
		}
	}

	Vector3 toCenter = obbB.center - obbA.center;

	for (int i = 0; i < 15; ++i)
	{
		const Vector3& axis = testAxes[i];
		if (axis.x == 0 && axis.y == 0 && axis.z == 0) continue; // 無効軸スキップ

		// 射影幅
		float aProj =
			std::abs(Vector3::Dot(axesA[0] * obbA.size.x, axis)) +
			std::abs(Vector3::Dot(axesA[1] * obbA.size.y, axis)) +
			std::abs(Vector3::Dot(axesA[2] * obbA.size.z, axis));

		float bProj =
			std::abs(Vector3::Dot(axesB[0] * obbB.size.x, axis)) +
			std::abs(Vector3::Dot(axesB[1] * obbB.size.y, axis)) +
			std::abs(Vector3::Dot(axesB[2] * obbB.size.z, axis));

		float distance = std::abs(Vector3::Dot(toCenter, axis));

		if (distance > aProj + bProj)
		{
			// 非衝突
			return false;
		}
	}
	// 衝突している場合、衝突した位置を設定
	ICollisionComponent* aNonConst = const_cast<OBBColliderComponent*>(a);
	ICollisionComponent* bNonConst = const_cast<OBBColliderComponent*>(b);
	aNonConst->SetCollisionPosition(obbA.center);
	bNonConst->SetCollisionPosition(obbB.center);

	// 衝突
	return true;
}

bool CollisionManager::CheckCollision(const AABBColliderComponent* a, const OBBColliderComponent* b)
{
	const AABB& aBox = a->GetAABB();
	const OBB& obb = b->GetOBB();

	// OBBの回転行列（各軸ベクトル）
	Matrix4x4 rot = obb.rotate;

	// OBBの軸ベクトル
	Vector3 axes[3] =
	{
		Vector3::Normalize(Vector3(rot.m[0][0], rot.m[0][1], rot.m[0][2])),
		Vector3::Normalize(Vector3(rot.m[1][0], rot.m[1][1], rot.m[1][2])),
		Vector3::Normalize(Vector3(rot.m[2][0], rot.m[2][1], rot.m[2][2]))
	};

	// AABBの中心座標をOBBのローカル空間に変換
	Vector3 toCenter = aBox.GetCenter() - obb.center;

	// AABBの半分のサイズ
	Vector3 aHalfSize = aBox.GetHalfSize();

	// 15の分離軸
	Vector3 testAxes[6];
	int axisCount = 0;

	// OBBの軸をテスト軸に追加
	for (int i = 0; i < 3; ++i) testAxes[axisCount++] = axes[i];

	// AABBの軸（X, Y, Z）をテスト軸に追加
	testAxes[axisCount++] = Vector3(1, 0, 0);
	testAxes[axisCount++] = Vector3(0, 1, 0);
	testAxes[axisCount++] = Vector3(0, 0, 1);

	// 各軸で分離軸テストを実行
	for (int i = 0; i < axisCount; ++i)
	{
		const Vector3& axis = testAxes[i];

		// AABBの射影幅
		float aProj = std::abs(Vector3::Dot(axis, Vector3(aHalfSize.x, 0.0f, 0.0f))) +
			std::abs(Vector3::Dot(axis, Vector3(0.0f, aHalfSize.y, 0.0f))) +
			std::abs(Vector3::Dot(axis, Vector3(0.0f, 0.0f, aHalfSize.z)));

		// OBBの射影幅
		float bProj = std::abs(Vector3::Dot(axes[0] * obb.size.x, axis)) +
			std::abs(Vector3::Dot(axes[1] * obb.size.y, axis)) +
			std::abs(Vector3::Dot(axes[2] * obb.size.z, axis));

		// AABBとOBBの中心の距離
		float distance = std::abs(Vector3::Dot(toCenter, axis));

		// 分離軸テスト（衝突していない場合は離れている）
		if (distance > aProj + bProj)
		{
			// 非衝突
			return false;
		}
	}

	// 衝突している場合、衝突した位置を設定
	ICollisionComponent* aNonConst = const_cast<AABBColliderComponent*>(a);
	ICollisionComponent* bNonConst = const_cast<OBBColliderComponent*>(b);
	aNonConst->SetCollisionPosition(aBox.GetCenter());
	aNonConst->SetCollisionPosition(obb.center);

	// 衝突している
	return true;
}

bool CollisionManager::CheckSubstepCollision(const AABBColliderComponent* a, const AABBColliderComponent* b)
{
	const Vector3& start = a->GetPreviousPosition();
	const Vector3& end = a->GetOwner()->GetPosition();
	const AABB& aBox = a->GetAABB();
	const AABB& bBox = b->GetAABB();

	// 静的判定（今フレームすでに重なってればOK）
	if (CheckCollision(a, b)) return true;

	Vector3 delta = end - start;
	float maxStep = 1.0f; // 1ステップあたりの最大移動量（調整可）
	int steps = (std::max)(1, static_cast<int>(delta.Length() / maxStep));

	for (int i = 1; i <= steps; ++i)
	{
		float t = static_cast<float>(i) / steps;
		Vector3 interpPos = MathUtils::Lerp(start, end, t);

		// 仮のAABBを移動後位置に作る
		AABB movedAABB(
			interpPos - aBox.GetHalfSize(),
			interpPos + aBox.GetHalfSize()
		);

		// bBoxをaBoxの半サイズ分膨張
		AABB expandedBox(
			bBox.min_ - aBox.GetHalfSize(),
			bBox.max_ + aBox.GetHalfSize()
		);

		// 判定
		if ((movedAABB.max_.x >= expandedBox.min_.x && movedAABB.min_.x <= expandedBox.max_.x) &&
			(movedAABB.max_.y >= expandedBox.min_.y && movedAABB.min_.y <= expandedBox.max_.y) &&
			(movedAABB.max_.z >= expandedBox.min_.z && movedAABB.min_.z <= expandedBox.max_.z))
		{
			return true;
		}
	}

	return false;
}

bool CollisionManager::CheckSubstepCollision(const OBBColliderComponent* a, const OBBColliderComponent* b)
{
	constexpr float MAX_STEP_DISTANCE = 1.0f; // 1サブステップあたり最大移動量（調整可）
	Vector3 start = a->GetPreviousPosition();
	Vector3 end = a->GetOwner()->GetPosition();
	OBB aObb = a->GetOBB();

	// サブステップ数を動的に決定
	float distance = (end - start).Length();
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(distance / MAX_STEP_DISTANCE)));

	// constを外して操作を行えるようにする
	OBBColliderComponent* aNonConst = const_cast<OBBColliderComponent*>(a);
	OBBColliderComponent* bNonConst = const_cast<OBBColliderComponent*>(b);


	for (int step = 0; step < subStepCount; ++step)
	{
		float t = static_cast<float>(step + 1) / subStepCount;
		Vector3 subPos = start + (end - start) * t;

		// aObbをサブステップ位置に仮想移動
		OBB movedOBB = aObb;
		movedOBB.center = subPos;

		OBBColliderComponent tempA(nullptr);
		tempA.SetOBB(movedOBB);
		if (CheckCollision(&tempA, b))
		{
			// 衝突が検出された場合、サブステップ位置を記録
			aNonConst->SetCollisionPosition(subPos);
			bNonConst->SetCollisionPosition(subPos);
			return true;
		}
	}
	return false;
}

bool CollisionManager::CheckSubstepCollision(const AABBColliderComponent* a, const OBBColliderComponent* b)
{
	// aの移動線分 vs bのOBBの外接AABB（aのAABBサイズ分膨張）で判定
	Vector3 start = a->GetPreviousPosition();
	Vector3 end = a->GetOwner()->GetPosition();
	OBB obb = b->GetOBB();
	const AABB& aBox = a->GetAABB();

	// OBBの外接AABBを計算
	Vector3 corners[8];
	int idx = 0;
	for (int x = -1; x <= 1; x += 2)
	{
		for (int y = -1; y <= 1; y += 2)
		{
			for (int z = -1; z <= 1; z += 2)
			{
				Vector3 local = Vector3((float)x, (float)y, (float)z) * obb.size;
				Vector4 local4(local.x, local.y, local.z, 1.0f);
				Vector4 rotated4 = obb.rotate * local4;
				Vector3 rotated(rotated4.x, rotated4.y, rotated4.z);
				Vector3 corner = obb.center + rotated;
				corners[idx++] = corner;
			}
		}
	}
	Vector3 minV = corners[0], maxV = corners[0];
	for (int i = 1; i < 8; ++i)
	{
		minV = Vector3::Min(minV, corners[i]);
		maxV = Vector3::Max(maxV, corners[i]);
	}
	Vector3 aHalf = aBox.GetHalfSize();
	AABB extAABB(minV - aHalf, maxV + aHalf);

	// 静的判定
	if (CheckCollision(a, b)) return true;

	Vector3 dir = end - start;
	float tmin = 0.0f, tmax = 1.0f;
	for (int i = 0; i < 3; ++i)
	{
		float dirComp, startComp, minComp, maxComp;
		switch (i)
		{
		case 0:
			dirComp = dir.x; startComp = start.x; minComp = extAABB.min_.x; maxComp = extAABB.max_.x;
			break;
		case 1:
			dirComp = dir.y; startComp = start.y; minComp = extAABB.min_.y; maxComp = extAABB.max_.y;
			break;
		case 2:
			dirComp = dir.z; startComp = start.z; minComp = extAABB.min_.z; maxComp = extAABB.max_.z;
			break;
		default:
			continue;
		}

		if (std::abs(dirComp) < 1e-8f)
		{
			if (startComp < minComp || startComp > maxComp) return false;
		}
		else
		{
			float ood = 1.0f / dirComp;
			float t1 = (minComp - startComp) * ood;
			float t2 = (maxComp - startComp) * ood;
			if (t1 > t2) std::swap(t1, t2);
			tmin = (std::max)(tmin, t1);
			tmax = std::min(tmax, t2);
			if (tmin > tmax) return false;
		}
	}
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
