#include "CollisionAlgorithm.h"
#include <cmath>
#include <algorithm>
#include "application/GameObject/base/GameObject.h"

// ============================================================================
// 定数定義
// ============================================================================

// サブステップ判定で使用する1ステップあたりの最大移動距離
constexpr float kMaxStepDistance = 1.0f;

// OBB判定で使用する分離軸の数
constexpr int kObbAxisCount = 3;          // 各OBBが持つ軸の数
constexpr int kObbTotalAxisCount = 15;    // 合計の分離軸数（3 + 3 + 9）

// 2D OBB判定で使用する分離軸の数
constexpr int kObb2dAxisCount = 4;        // 2D OBBの分離軸数

// AABB vs OBB判定で使用する分離軸の数
constexpr int kAabbObbAxisCount = 6;      // AABBとOBBの分離軸数

// 配列インデックス（軸の識別）
constexpr int kAxisX = 0;
constexpr int kAxisY = 1;
constexpr int kAxisZ = 2;

// --- 3D用判定 ---

/**
 * @brief AABB同士の3D衝突判定
 * 
 * 判定方法: 各軸（X, Y, Z）で境界が重なっているかを確認
 */
bool CollisionAlgorithm::CheckAABBvsAABB3D(const AABBColliderComponent* a, const AABBColliderComponent* b)
{
	const AABB& aBox = a->GetAABB();
	const AABB& bBox = b->GetAABB();

	// 各軸で重なりをチェック（全軸で重なっていれば衝突）
	return (aBox.max_.x >= bBox.min_.x && aBox.min_.x <= bBox.max_.x) &&
		(aBox.max_.y >= bBox.min_.y && aBox.min_.y <= bBox.max_.y) &&
		(aBox.max_.z >= bBox.min_.z && aBox.min_.z <= bBox.max_.z);
}

/**
 * @brief OBB同士の3D衝突判定（分離軸定理）
 * 
 * 判定方法: 分離軸定理（SAT: Separating Axis Theorem）
 * - 15の分離軸（各OBBの3軸 + 外積で生成される9軸）で判定
 * - 全ての軸で投影範囲が重なっていれば衝突
 * - 1つでも分離軸が見つかれば衝突していない
 */
bool CollisionAlgorithm::CheckOBBvsOBB3D(const OBBColliderComponent* a, const OBBColliderComponent* b)
{
	const OBB& obbA = a->GetOBB();
	const OBB& obbB = b->GetOBB();

	Matrix4x4 rotA = obbA.rotate;
	Matrix4x4 rotB = obbB.rotate;

	// 各OBBのワールド軸ベクトルを取得（回転行列の各行を正規化）
	Vector3 axesA[kObbAxisCount] =
	{
		Vector3::Normalize(Vector3(rotA.m[kAxisX][kAxisX], rotA.m[kAxisX][kAxisY], rotA.m[kAxisX][kAxisZ])),
		Vector3::Normalize(Vector3(rotA.m[kAxisY][kAxisX], rotA.m[kAxisY][kAxisY], rotA.m[kAxisY][kAxisZ])),
		Vector3::Normalize(Vector3(rotA.m[kAxisZ][kAxisX], rotA.m[kAxisZ][kAxisY], rotA.m[kAxisZ][kAxisZ]))
	};

	Vector3 axesB[kObbAxisCount] =
	{
		Vector3::Normalize(Vector3(rotB.m[kAxisX][kAxisX], rotB.m[kAxisX][kAxisY], rotB.m[kAxisX][kAxisZ])),
		Vector3::Normalize(Vector3(rotB.m[kAxisY][kAxisX], rotB.m[kAxisY][kAxisY], rotB.m[kAxisY][kAxisZ])),
		Vector3::Normalize(Vector3(rotB.m[kAxisZ][kAxisX], rotB.m[kAxisZ][kAxisY], rotB.m[kAxisZ][kAxisZ]))
	};

	// 15の分離軸を構築（Aの3軸 + Bの3軸 + 外積9軸）
	Vector3 testAxes[kObbTotalAxisCount];
	int axisCount = 0;

	// OBB Aの軸を追加
	for (int i = 0; i < kObbAxisCount; ++i) testAxes[axisCount++] = axesA[i];
	// OBB Bの軸を追加
	for (int i = 0; i < kObbAxisCount; ++i) testAxes[axisCount++] = axesB[i];

	// 外積で生成される9軸を追加（各軸の組み合わせ）
	for (int i = 0; i < kObbAxisCount; ++i)
	{
		for (int j = 0; j < kObbAxisCount; ++j)
		{
			testAxes[axisCount++] = Vector3::Normalize(Vector3::Cross(axesA[i], axesB[j]));
		}
	}

	// 2つのOBB中心間のベクトル
	Vector3 toCenter = obbB.center - obbA.center;

	// 分離軸定理（SAT）で判定：全ての軸で投影範囲をチェック
	for (int i = 0; i < kObbTotalAxisCount; ++i)
	{
		const Vector3& axis = testAxes[i];
		
		// ゼロベクトル（平行な軸の外積）はスキップ
		if (axis.x == 0 && axis.y == 0 && axis.z == 0) continue;

		// 各OBBの軸への投影サイズを計算（半幅の合計）
		float aProj =
			std::abs(Vector3::Dot(axesA[kAxisX] * obbA.size.x, axis)) +
			std::abs(Vector3::Dot(axesA[kAxisY] * obbA.size.y, axis)) +
			std::abs(Vector3::Dot(axesA[kAxisZ] * obbA.size.z, axis));

		float bProj =
			std::abs(Vector3::Dot(axesB[kAxisX] * obbB.size.x, axis)) +
			std::abs(Vector3::Dot(axesB[kAxisY] * obbB.size.y, axis)) +
			std::abs(Vector3::Dot(axesB[kAxisZ] * obbB.size.z, axis));

		// 中心間距離の軸への投影
		float distance = std::abs(Vector3::Dot(toCenter, axis));

		// 分離軸が見つかった場合は衝突していない
		if (distance > aProj + bProj)
		{
			return false;
		}
	}
	
	// 衝突位置を記録
	ICollisionComponent* aNonConst = const_cast<OBBColliderComponent*>(a);
	ICollisionComponent* bNonConst = const_cast<OBBColliderComponent*>(b);
	aNonConst->SetCollisionPosition(obbA.center);
	bNonConst->SetCollisionPosition(obbB.center);

	return true;
}

/**
 * @brief AABBとOBBの3D衝突判定
 * 
 * 判定方法: 分離軸定理（SAT）
 * - AABBを単位軸（X, Y, Z）を持つOBBとして扱う
 * - 6の分離軸（OBBの3軸 + 単位軸3軸）で判定
 */
bool CollisionAlgorithm::CheckAABBvsOBB3D(const AABBColliderComponent* a, const OBBColliderComponent* b)
{
	const AABB& aBox = a->GetAABB();
	const OBB& obb = b->GetOBB();

	Matrix4x4 rot = obb.rotate;

	// OBBの軸ベクトルを取得
	Vector3 axes[kObbAxisCount] =
	{
		Vector3::Normalize(Vector3(rot.m[kAxisX][kAxisX], rot.m[kAxisX][kAxisY], rot.m[kAxisX][kAxisZ])),
		Vector3::Normalize(Vector3(rot.m[kAxisY][kAxisX], rot.m[kAxisY][kAxisY], rot.m[kAxisY][kAxisZ])),
		Vector3::Normalize(Vector3(rot.m[kAxisZ][kAxisX], rot.m[kAxisZ][kAxisY], rot.m[kAxisZ][kAxisZ]))
	};

	// AABB中心からOBB中心へのベクトル
	Vector3 toCenter = aBox.GetCenter() - obb.center;
	Vector3 aHalfSize = aBox.GetHalfSize();

	// 分離軸を構築（OBBの3軸 + 単位軸3軸）
	Vector3 testAxes[kAabbObbAxisCount];
	int axisCount = 0;

	// OBBの軸を追加
	for (int i = 0; i < kObbAxisCount; ++i) testAxes[axisCount++] = axes[i];
	// 単位軸（AABBの軸）を追加
	testAxes[axisCount++] = Vector3(1, 0, 0);
	testAxes[axisCount++] = Vector3(0, 1, 0);
	testAxes[axisCount++] = Vector3(0, 0, 1);

	// 各分離軸で判定
	for (int i = 0; i < axisCount; ++i)
	{
		const Vector3& axis = testAxes[i];

		// AABBの投影サイズを計算
		float aProj = std::abs(Vector3::Dot(axis, Vector3(aHalfSize.x, 0.0f, 0.0f))) +
			std::abs(Vector3::Dot(axis, Vector3(0.0f, aHalfSize.y, 0.0f))) +
			std::abs(Vector3::Dot(axis, Vector3(0.0f, 0.0f, aHalfSize.z)));

		// OBBの投影サイズを計算
		float bProj = std::abs(Vector3::Dot(axes[kAxisX] * obb.size.x, axis)) +
			std::abs(Vector3::Dot(axes[kAxisY] * obb.size.y, axis)) +
			std::abs(Vector3::Dot(axes[kAxisZ] * obb.size.z, axis));

		float distance = std::abs(Vector3::Dot(toCenter, axis));

		// 分離軸が見つかった場合は衝突していない
		if (distance > aProj + bProj)
		{
			return false;
		}
	}

	// 衝突位置を記録
	ICollisionComponent* aNonConst = const_cast<AABBColliderComponent*>(a);
	ICollisionComponent* bNonConst = const_cast<OBBColliderComponent*>(b);
	aNonConst->SetCollisionPosition(aBox.GetCenter());
	bNonConst->SetCollisionPosition(obb.center);

	return true;
}

/**
 * @brief Sphere同士の3D衝突判定
 * 
 * 判定方法: 中心間距離と半径の和を比較
 * - 距離の2乗 <= 半径の和の2乗 なら衝突
 */
bool CollisionAlgorithm::CheckSpherevsSphere3D(const SphereColliderComponent* a, const SphereColliderComponent* b)
{
	const Sphere& sA = a->GetSphere();
	const Sphere& sB = b->GetSphere();

	// 中心間距離の2乗を計算（平方根を避けて高速化）
	float distSq = (sA.center - sB.center).LengthSquared();
	float radiusSum = sA.radius + sB.radius;

	// 距離 <= 半径の和 なら衝突
	if (distSq <= radiusSum * radiusSum)
	{
		// 衝突位置を記録
		ICollisionComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
		ICollisionComponent* bNonConst = const_cast<SphereColliderComponent*>(b);
		aNonConst->SetCollisionPosition(sA.center);
		bNonConst->SetCollisionPosition(sB.center);
		return true;
	}
	return false;
}

/**
 * @brief SphereとAABBの3D衝突判定
 * 
 * 判定方法: AABB上の最近傍点との距離を判定
 * - AABB上でSphere中心に最も近い点を計算
 * - その点とSphere中心の距離が半径以下なら衝突
 */
bool CollisionAlgorithm::CheckSpherevsAABB3D(const SphereColliderComponent* a, const AABBColliderComponent* b)
{
	const Sphere& s = a->GetSphere();
	const AABB& box = b->GetAABB();

	// AABB上の最近傍点を計算（各軸でクランプ）
	Vector3 closest(
		(std::max)(box.min_.x, (std::min)(s.center.x, box.max_.x)),
		(std::max)(box.min_.y, (std::min)(s.center.y, box.max_.y)),
		(std::max)(box.min_.z, (std::min)(s.center.z, box.max_.z))
	);
	
	// 最近傍点とSphere中心の距離を計算
	float distSq = (s.center - closest).LengthSquared();

	// 距離 <= 半径 なら衝突
	if (distSq <= s.radius * s.radius)
	{
		// 衝突位置を記録
		ICollisionComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
		ICollisionComponent* bNonConst = const_cast<AABBColliderComponent*>(b);
		aNonConst->SetCollisionPosition(s.center);
		bNonConst->SetCollisionPosition(closest);
		return true;
	}
	return false;
}

/**
 * @brief SphereとOBBの3D衝突判定
 * 
 * 判定方法: OBB上の最近傍点との距離を判定
 * - OBBローカル空間でSphere中心に最も近い点を計算
 * - その点とSphere中心の距離が半径以下なら衝突
 */
bool CollisionAlgorithm::CheckSpherevsOBB3D(const SphereColliderComponent* a, const OBBColliderComponent* b)
{
	const Sphere& s = a->GetSphere();
	const OBB& obb = b->GetOBB();

	// Sphere中心からOBB中心へのベクトル
	Vector3 d = s.center - obb.center;
	Vector3 closest = obb.center;

	const float sizes[kObbAxisCount] = { obb.size.x, obb.size.y, obb.size.z };

	// OBB上の最近点を計算（各軸に沿って投影・クランプ）
	for (int i = 0; i < kObbAxisCount; ++i)
	{
		// OBBの各軸を取得
		Vector3 axis(obb.rotate.m[i][kAxisX], obb.rotate.m[i][kAxisY], obb.rotate.m[i][kAxisZ]);
		// ベクトルdを軸に投影
		float dist = Vector3::Dot(d, axis);
		// OBBの半幅でクランプ
		float clamped = (std::max)(-sizes[i], (std::min)(dist, sizes[i]));
		// 最近傍点を更新
		closest += axis * clamped;
	}
	
	// 最近傍点とSphere中心の距離を計算
	float distSq = (s.center - closest).LengthSquared();

	// 距離 <= 半径 なら衝突
	if (distSq <= s.radius * s.radius)
	{
		// 衝突位置を記録
		ICollisionComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
		ICollisionComponent* bNonConst = const_cast<OBBColliderComponent*>(b);
		aNonConst->SetCollisionPosition(s.center);
		bNonConst->SetCollisionPosition(closest);
		return true;
	}
	return false;
}

// --- 3Dサブステップ判定 ---

/**
 * @brief AABB同士のサブステップ3D衝突判定
 * 
 * 判定方法: 前フレーム位置から現在位置までを線分補間して判定
 * - 移動距離に応じてサブステップ数を決定
 * - 各ステップでAABBを配置して判定
 * - 高速移動する弾丸などのすり抜けを防止
 */
bool CollisionAlgorithm::CheckAABBvsAABBSubstep3D(const AABBColliderComponent* a, const AABBColliderComponent* b)
{
	// 前フレームと現在フレームの位置を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const AABB& aBox = a->GetAABB();
	const AABB& bBox = b->GetAABB();

	// まず現在位置での判定を試行
	if (CheckAABBvsAABB3D(a, b)) return true;

	// 移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	float maxDistance = (std::max)(distanceA, distanceB);
	// 移動距離に応じてサブステップ数を決定（すり抜け防止）
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	AABBColliderComponent* aNonConst = const_cast<AABBColliderComponent*>(a);
	AABBColliderComponent* bNonConst = const_cast<AABBColliderComponent*>(b);

	AABBColliderComponent tempA(nullptr);
	AABBColliderComponent tempB(nullptr);

	// 前フレームから現在位置までを線分補間して判定
	for (int step = 0; step <= subStepCount; ++step)
	{
		// 補間係数t（0.0〜1.0）
		float t = static_cast<float>(step) / subStepCount;

		// 補間位置を計算
		Vector3 subPosA = MathUtils::Lerp(startA, endA, t);
		Vector3 subPosB = MathUtils::Lerp(startB, endB, t);

		// 補間位置でAABBを構築
		AABB movedAABB_A(subPosA - aBox.GetHalfSize(), subPosA + aBox.GetHalfSize());
		AABB movedAABB_B(subPosB - bBox.GetHalfSize(), subPosB + bBox.GetHalfSize());


		tempA.SetAABB(movedAABB_A);
		tempB.SetAABB(movedAABB_B);

		// 衝突判定
		if (CheckAABBvsAABB3D(&tempA, &tempB))
		{
			// 衝突位置を記録
			aNonConst->SetCollisionPosition(subPosA);
			bNonConst->SetCollisionPosition(subPosB);
			return true;
		}
	}

	return false;
}

/**
 * @brief OBB同士のサブステップ3D衝突判定
 * 
 * 判定方法: 前フレーム位置から現在位置までを線分補間して判定
 */
bool CollisionAlgorithm::CheckOBBvsOBBSubstep3D(const OBBColliderComponent* a, const OBBColliderComponent* b)
{
	// 前フレームと現在フレームの位置を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	OBB aObb = a->GetOBB();
	OBB bObb = b->GetOBB();

	// 移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	float maxDistance = (std::max)(distanceA, distanceB);
	// 移動距離に応じてサブステップ数を決定
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	OBBColliderComponent* aNonConst = const_cast<OBBColliderComponent*>(a);
	OBBColliderComponent* bNonConst = const_cast<OBBColliderComponent*>(b);

	OBBColliderComponent tempA(nullptr);
	OBBColliderComponent tempB(nullptr);

	// 各サブステップで判定
	for (int step = 0; step < subStepCount; ++step)
	{
		// 補間係数t
		float t = static_cast<float>(step + 1) / subStepCount;

		// 補間位置を計算
		Vector3 subPosA = startA + (endA - startA) * t;
		Vector3 subPosB = startB + (endB - startB) * t;

		// 補間位置でOBBを構築
		OBB movedOBB_A = aObb;
		OBB movedOBB_B = bObb;
		movedOBB_A.center = subPosA;
		movedOBB_B.center = subPosB;


		tempA.SetOBB(movedOBB_A);
		tempB.SetOBB(movedOBB_B);

		// 衝突判定
		if (CheckOBBvsOBB3D(&tempA, &tempB))
		{
			// 衝突位置を記録
			aNonConst->SetCollisionPosition(subPosA);
			bNonConst->SetCollisionPosition(subPosB);
			return true;
		}
	}

	return false;
}

/**
 * @brief AABBとOBBのサブステップ3D衝突判定
 * 
 * 判定方法: 前フレーム位置から現在位置までを線分補間して判定
 */
bool CollisionAlgorithm::CheckAABBvsOBBSubstep3D(const AABBColliderComponent* a, const OBBColliderComponent* b)
{
	// 前フレームと現在フレームの位置を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const AABB& aBox = a->GetAABB();
	OBB bObb = b->GetOBB();

	// 移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	float maxDistance = (std::max)(distanceA, distanceB);
	// 移動距離に応じてサブステップ数を決定
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	AABBColliderComponent* aNonConst = const_cast<AABBColliderComponent*>(a);
	OBBColliderComponent* bNonConst = const_cast<OBBColliderComponent*>(b);

	AABBColliderComponent tempA(nullptr);
	OBBColliderComponent tempB(nullptr);

	// 各サブステップで判定
	for (int step = 0; step < subStepCount; ++step)
	{
		// 補間係数t
		float t = static_cast<float>(step + 1) / subStepCount;

		// 補間位置を計算
		Vector3 subPosA = startA + (endA - startA) * t;
		Vector3 subPosB = startB + (endB - startB) * t;

		// 補間位置でコライダーを構築
		OBB movedOBB = bObb;
		movedOBB.center = subPosB;

		Vector3 aHalf = aBox.GetHalfSize();
		AABB movedAABB(subPosA - aHalf, subPosA + aHalf);


		tempA.SetAABB(movedAABB);
		tempB.SetOBB(movedOBB);

		// 衝突判定
		if (CheckAABBvsOBB3D(&tempA, &tempB))
		{
			// 衝突位置を記録
			aNonConst->SetCollisionPosition(subPosA);
			bNonConst->SetCollisionPosition(subPosB);
			return true;
		}
	}

	return false;
}


/**
 * @brief Sphere同士のサブステップ3D衝突判定
 * 
 * 判定方法: 前フレーム位置から現在位置までを線分補間して判定
 */
bool CollisionAlgorithm::CheckSpherevsSphereSubstep3D(const SphereColliderComponent* a, const SphereColliderComponent* b)
{
	// 前フレームと現在フレームの位置を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const Sphere& sphereA = a->GetSphere();
	const Sphere& sphereB = b->GetSphere();

	// まず現在位置での静的判定を試行
	if (CheckSpherevsSphere3D(a, b)) return true;

	// 移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	float maxDistance = (std::max)(distanceA, distanceB);
	// 移動距離に応じてサブステップ数を決定
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	SphereColliderComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
	SphereColliderComponent* bNonConst = const_cast<SphereColliderComponent*>(b);

	// 各サブステップで判定
	for (int step = 1; step <= subStepCount; ++step)
	{
		// 補間係数t
		float t = static_cast<float>(step) / subStepCount;
		
		// 補間位置を計算
		Vector3 subPosA = MathUtils::Lerp(startA, endA, t);
		Vector3 subPosB = MathUtils::Lerp(startB, endB, t);

		// 補間位置で球を構築
		Sphere tempA(subPosA, sphereA.radius);
		Sphere tempB(subPosB, sphereB.radius);

		// 中心間距離を計算して衝突判定
		float distSq = (subPosA - subPosB).LengthSquared();
		float radiusSum = tempA.radius + tempB.radius;

		if (distSq <= radiusSum * radiusSum)
		{
			// 衝突位置を記録
			aNonConst->SetCollisionPosition(subPosA);
			bNonConst->SetCollisionPosition(subPosB);
			return true;
		}
	}
	return false;
}

/**
 * @brief SphereとAABBのサブステップ3D衝突判定
 * 
 * 判定方法: 前フレーム位置から現在位置までを線分補間して判定
 */
bool CollisionAlgorithm::CheckSpherevsAABBSubstep3D(const SphereColliderComponent* a, const AABBColliderComponent* b)
{
	// 前フレームと現在フレームの位置を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const Sphere& sphereA = a->GetSphere();
	const AABB& boxB = b->GetAABB();

	// まず現在位置での静的判定を試行
	if (CheckSpherevsAABB3D(a, b)) return true;

	// 移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	float maxDistance = (std::max)(distanceA, distanceB);
	// 移動距離に応じてサブステップ数を決定
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	SphereColliderComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
	AABBColliderComponent* bNonConst = const_cast<AABBColliderComponent*>(b);

	// 各サブステップで判定
	for (int step = 1; step <= subStepCount; ++step)
	{
		// 補間係数t
		float t = static_cast<float>(step) / subStepCount;
		
		// 補間位置を計算
		Vector3 subPosA = MathUtils::Lerp(startA, endA, t);
		Vector3 subPosB = MathUtils::Lerp(startB, endB, t);

		// 補間位置でコライダーを構築
		Sphere tempSphere(subPosA, sphereA.radius);
		Vector3 bHalf = boxB.GetHalfSize();
		AABB movedAABB(subPosB - bHalf, subPosB + bHalf);

		// AABB上の最近傍点を計算
		Vector3 closest(
			(std::max)(movedAABB.min_.x, (std::min)(tempSphere.center.x, movedAABB.max_.x)),
			(std::max)(movedAABB.min_.y, (std::min)(tempSphere.center.y, movedAABB.max_.y)),
			(std::max)(movedAABB.min_.z, (std::min)(tempSphere.center.z, movedAABB.max_.z))
		);
		
		// 最近傍点との距離を計算
		float distSq = (tempSphere.center - closest).LengthSquared();

		// 距離 <= 半径 なら衝突
		if (distSq <= tempSphere.radius * tempSphere.radius)
		{
			// 衝突位置を記録
			aNonConst->SetCollisionPosition(tempSphere.center);
			bNonConst->SetCollisionPosition(closest);
			return true;
		}
	}
	return false;
}

/**
 * @brief SphereとOBBのサブステップ3D衝突判定
 * 
 * 判定方法: 前フレーム位置から現在位置までを線分補間して判定
 */
bool CollisionAlgorithm::CheckSpherevsOBBSubstep3D(const SphereColliderComponent* a, const OBBColliderComponent* b)
{
	// 前フレームと現在フレームの位置を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const Sphere& sphereA = a->GetSphere();
	OBB obbB = b->GetOBB();

	// まず現在位置での静的判定を試行
	if (CheckSpherevsOBB3D(a, b)) return true;

	// 移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	float maxDistance = (std::max)(distanceA, distanceB);
	// 移動距離に応じてサブステップ数を決定
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	SphereColliderComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
	OBBColliderComponent* bNonConst = const_cast<OBBColliderComponent*>(b);

	// 各サブステップで判定
	for (int step = 1; step <= subStepCount; ++step)
	{
		// 補間係数t
		float t = static_cast<float>(step) / subStepCount;
		
		// 補間位置を計算
		Vector3 subPosA = MathUtils::Lerp(startA, endA, t);
		Vector3 subPosB = MathUtils::Lerp(startB, endB, t);

		// 補間位置でコライダーを構築
		Sphere tempSphere(subPosA, sphereA.radius);
		OBB movedOBB = obbB;
		movedOBB.center = subPosB;

		// OBB上の最近傍点を計算（OBBローカル空間への変換）
		Vector3 d = tempSphere.center - movedOBB.center;
		Vector3 closest = movedOBB.center;

		const float sizes[kObbAxisCount] = { movedOBB.size.x, movedOBB.size.y, movedOBB.size.z };
		
		// 各軸に沿って投影・クランプ
		for (int i = 0; i < kObbAxisCount; ++i)
		{
			Vector3 axis(movedOBB.rotate.m[i][kAxisX], movedOBB.rotate.m[i][kAxisY], movedOBB.rotate.m[i][kAxisZ]);
			float dist = Vector3::Dot(d, axis);
			float clamped = (std::max)(-sizes[i], (std::min)(dist, sizes[i]));
			closest += axis * clamped;
		}
		
		// 最近傍点との距離を計算
		float distSq = (tempSphere.center - closest).LengthSquared();

		// 距離 <= 半径 なら衝突
		if (distSq <= tempSphere.radius * tempSphere.radius)
		{
			// 衝突位置を記録
			aNonConst->SetCollisionPosition(tempSphere.center);
			bNonConst->SetCollisionPosition(closest);
			return true;
		}
	}
	return false;
}

// --- 2D用判定（平面指定） ---

/**
 * @brief 平面の軸インデックスを取得
 * 
 * 指定された平面に対応する2つの軸インデックスを取得します。
 * 
 * @param plane 判定平面（XY, XZ, YZ）
 * @param axis1 [out] 第1軸のインデックス
 * @param axis2 [out] 第2軸のインデックス
 */
static void GetPlaneAxes(CollisionPlane plane, int& axis1, int& axis2)
{
	switch (plane)
	{
	case CollisionPlane::XY: axis1 = kAxisX; axis2 = kAxisY; break;
	case CollisionPlane::XZ: axis1 = kAxisX; axis2 = kAxisZ; break;
	case CollisionPlane::YZ: axis1 = kAxisY; axis2 = kAxisZ; break;
	}
}

/**
 * @brief 軸インデックスからベクトル成分を取得
 * 
 * @param v 取得元のベクトル
 * @param axis 軸インデックス（0:X, 1:Y, 2:Z）
 * @return 指定された軸の値
 */
float GetSizeFromIndex(const Vector3& v, int axis)
{
	switch (axis)
	{
	case kAxisX: return v.x;
	case kAxisY: return v.y;
	case kAxisZ: return v.z;
	default: return 0.0f;
	}
}

// --- AABB vs AABB 2D判定 ---

/**
 * @brief AABB同士の2D衝突判定
 * 
 * 判定方法: 指定平面上の2軸で境界が重なっているかを確認
 */
bool CollisionAlgorithm::CheckAABBvsAABB2D(const AABBColliderComponent* a, const AABBColliderComponent* b, CollisionPlane plane)
{
	// 判定平面の軸インデックスを取得
	int axis1, axis2;
	GetPlaneAxes(plane, axis1, axis2);

	const AABB& aBox = a->GetAABB();
	const AABB& bBox = b->GetAABB();

	// 各軸のmin/maxを取得
	float aMin1 = GetSizeFromIndex(aBox.min_, axis1);
	float aMax1 = GetSizeFromIndex(aBox.max_, axis1);
	float aMin2 = GetSizeFromIndex(aBox.min_, axis2);
	float aMax2 = GetSizeFromIndex(aBox.max_, axis2);

	float bMin1 = GetSizeFromIndex(bBox.min_, axis1);
	float bMax1 = GetSizeFromIndex(bBox.max_, axis1);
	float bMin2 = GetSizeFromIndex(bBox.min_, axis2);
	float bMax2 = GetSizeFromIndex(bBox.max_, axis2);

	// 両軸で重なりをチェック
	bool overlap =
		(aMax1 >= bMin1 && aMin1 <= bMax1) &&
		(aMax2 >= bMin2 && aMin2 <= bMax2);

	if (overlap)
	{
		// 衝突位置を記録
		ICollisionComponent* aNonConst = const_cast<AABBColliderComponent*>(a);
		ICollisionComponent* bNonConst = const_cast<AABBColliderComponent*>(b);
		aNonConst->SetCollisionPosition(aBox.GetCenter());
		bNonConst->SetCollisionPosition(bBox.GetCenter());
	}
	return overlap;
}

// --- OBB vs OBB 2D判定 ---

/**
 * @brief OBB同士の2D衝突判定
 * 
 * 判定方法: 指定平面上で分離軸定理（SAT）を使用
 */
bool CollisionAlgorithm::CheckOBBvsOBB2D(const OBBColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane)
{
	const OBB& obbA = a->GetOBB();
	const OBB& obbB = b->GetOBB();

	bool isColliding = false;

	// 平面に応じた専用判定関数を呼び出し
	switch (plane)
	{
	case CollisionPlane::XY:
		isColliding = CheckOBBvsOBB_XY(obbA, obbB);
		break;
	case CollisionPlane::XZ:
		isColliding = CheckOBBvsOBB_XZ(obbA, obbB);
		break;
	case CollisionPlane::YZ:
		isColliding = CheckOBBvsOBB_YZ(obbA, obbB);
		break;
	}

	if (isColliding)
	{
		// 衝突位置を記録
		ICollisionComponent* aNonConst = const_cast<OBBColliderComponent*>(a);
		ICollisionComponent* bNonConst = const_cast<OBBColliderComponent*>(b);
		aNonConst->SetCollisionPosition(obbA.center);
		bNonConst->SetCollisionPosition(obbB.center);
	}

	return isColliding;
}

/**
 * @brief XY平面専用のOBB衝突判定（高速版）
 * 
 * 判定方法: 2D分離軸定理（4つの分離軸で判定）
 */
bool CollisionAlgorithm::CheckOBBvsOBB_XY(const OBB& obbA, const OBB& obbB)
{
	// XY平面の中心座標を取得
	Vector2 centerA(obbA.center.x, obbA.center.y);
	Vector2 centerB(obbB.center.x, obbB.center.y);
	Vector2 toCenter = centerB - centerA;

	// XY平面の軸ベクトル（回転行列から直接取得）
	Vector2 axesA[2] = {
		Vector2(obbA.rotate.m[kAxisX][kAxisX], obbA.rotate.m[kAxisX][kAxisY]),  // X軸
		Vector2(obbA.rotate.m[kAxisY][kAxisX], obbA.rotate.m[kAxisY][kAxisY])   // Y軸
	};

	Vector2 axesB[2] = {
		Vector2(obbB.rotate.m[kAxisX][kAxisX], obbB.rotate.m[kAxisX][kAxisY]),  // X軸
		Vector2(obbB.rotate.m[kAxisY][kAxisX], obbB.rotate.m[kAxisY][kAxisY])   // Y軸
	};

	// XY平面のサイズ
	Vector2 sizeA(obbA.size.x, obbA.size.y);
	Vector2 sizeB(obbB.size.x, obbB.size.y);

	// 4つの分離軸でテスト（各OBBの2軸）
	Vector2 testAxes[kObb2dAxisCount] = { axesA[0], axesA[1], axesB[0], axesB[1] };

	for (int i = 0; i < kObb2dAxisCount; ++i)
	{
		const Vector2& axis = testAxes[i];

		// 各OBBの投影幅を計算
		float projA = std::abs(Vector2::Dot(axesA[0] * sizeA.x, axis)) +
			std::abs(Vector2::Dot(axesA[1] * sizeA.y, axis));

		float projB = std::abs(Vector2::Dot(axesB[0] * sizeB.x, axis)) +
			std::abs(Vector2::Dot(axesB[1] * sizeB.y, axis));

		// 中心間距離の投影
		float distance = std::abs(Vector2::Dot(toCenter, axis));

		// 分離軸が見つかった場合は衝突していない
		if (distance > projA + projB)
		{
			return false;
		}
	}
	return true;
}

/**
 * @brief XZ平面専用のOBB衝突判定（高速版）
 * 
 * 判定方法: 2D分離軸定理（4つの分離軸で判定）
 */
bool CollisionAlgorithm::CheckOBBvsOBB_XZ(const OBB& obbA, const OBB& obbB)
{
	// XZ平面の中心座標を取得
	Vector2 centerA(obbA.center.x, obbA.center.z);
	Vector2 centerB(obbB.center.x, obbB.center.z);
	Vector2 toCenter = centerB - centerA;

	// XZ平面の軸ベクトル（回転行列から取得）
	Vector2 axesA[2] = {
		Vector2(obbA.rotate.m[kAxisX][kAxisX], obbA.rotate.m[kAxisX][kAxisZ]),  // X軸のXZ成分
		Vector2(obbA.rotate.m[kAxisZ][kAxisX], obbA.rotate.m[kAxisZ][kAxisZ])   // Z軸のXZ成分
	};

	Vector2 axesB[2] = {
		Vector2(obbB.rotate.m[kAxisX][kAxisX], obbB.rotate.m[kAxisX][kAxisZ]),
		Vector2(obbB.rotate.m[kAxisZ][kAxisX], obbB.rotate.m[kAxisZ][kAxisZ])
	};

	// XZ平面のサイズ（X成分とZ成分）
	Vector2 sizeA(obbA.size.x, obbA.size.z);
	Vector2 sizeB(obbB.size.x, obbB.size.z);

	// 4つの分離軸でテスト
	Vector2 testAxes[kObb2dAxisCount] = { axesA[0], axesA[1], axesB[0], axesB[1] };

	for (int i = 0; i < kObb2dAxisCount; ++i)
	{
		const Vector2& axis = testAxes[i];

		// 各OBBの投影幅を計算
		float projA = std::abs(Vector2::Dot(axesA[0] * sizeA.x, axis)) +
			std::abs(Vector2::Dot(axesA[1] * sizeA.y, axis));

		float projB = std::abs(Vector2::Dot(axesB[0] * sizeB.x, axis)) +
			std::abs(Vector2::Dot(axesB[1] * sizeB.y, axis));

		float distance = std::abs(Vector2::Dot(toCenter, axis));

		// 分離軸が見つかった場合は衝突していない
		if (distance > projA + projB)
		{
			return false;
		}
	}
	return true;
}

/**
 * @brief YZ平面専用のOBB衝突判定（高速版）
 * 
 * 判定方法: 2D分離軸定理（4つの分離軸で判定）
 */
bool CollisionAlgorithm::CheckOBBvsOBB_YZ(const OBB& obbA, const OBB& obbB)
{
	// YZ平面の中心座標を取得
	Vector2 centerA(obbA.center.y, obbA.center.z);
	Vector2 centerB(obbB.center.y, obbB.center.z);
	Vector2 toCenter = centerB - centerA;

	// YZ平面の軸ベクトル（回転行列から取得）
	Vector2 axesA[2] = {
		Vector2(obbA.rotate.m[kAxisY][kAxisY], obbA.rotate.m[kAxisY][kAxisZ]),  // Y軸のYZ成分
		Vector2(obbA.rotate.m[kAxisZ][kAxisY], obbA.rotate.m[kAxisZ][kAxisZ])   // Z軸のYZ成分
	};

	Vector2 axesB[2] = {
		Vector2(obbB.rotate.m[kAxisY][kAxisY], obbB.rotate.m[kAxisY][kAxisZ]),
		Vector2(obbB.rotate.m[kAxisZ][kAxisY], obbB.rotate.m[kAxisZ][kAxisZ])
	};

	// YZ平面のサイズ（Y成分とZ成分）
	Vector2 sizeA(obbA.size.y, obbA.size.z);
	Vector2 sizeB(obbB.size.y, obbB.size.z);

	// 4つの分離軸でテスト
	Vector2 testAxes[kObb2dAxisCount] = { axesA[0], axesA[1], axesB[0], axesB[1] };

	for (int i = 0; i < kObb2dAxisCount; ++i)
	{
		const Vector2& axis = testAxes[i];

		// 各OBBの投影幅を計算
		float projA = std::abs(Vector2::Dot(axesA[0] * sizeA.x, axis)) +
			std::abs(Vector2::Dot(axesA[1] * sizeA.y, axis));

		float projB = std::abs(Vector2::Dot(axesB[0] * sizeB.x, axis)) +
			std::abs(Vector2::Dot(axesB[1] * sizeB.y, axis));

		float distance = std::abs(Vector2::Dot(toCenter, axis));

		// 分離軸が見つかった場合は衝突していない
		if (distance > projA + projB)
		{
			return false;
		}
	}
	return true;
}

// --- AABB vs OBB 2D判定 ---

/**
 * @brief AABBとOBBの2D衝突判定
 * 
 * 判定方法: 指定平面上で分離軸定理（SAT）を使用
 */
bool CollisionAlgorithm::CheckAABBvsOBB2D(const AABBColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane)
{
	// 判定平面の軸インデックスを取得
	int axis1, axis2;
	GetPlaneAxes(plane, axis1, axis2);

	const AABB& aBox = a->GetAABB();
	const OBB& obb = b->GetOBB();

	// 2D中心座標を平面に応じて取得
	Vector2 aCenter, obbCenter;
	switch (plane)
	{
	case CollisionPlane::XY:
		aCenter = Vector2(aBox.GetCenter().x, aBox.GetCenter().y);
		obbCenter = Vector2(obb.center.x, obb.center.y);
		break;
	case CollisionPlane::XZ:
		aCenter = Vector2(aBox.GetCenter().x, aBox.GetCenter().z);
		obbCenter = Vector2(obb.center.x, obb.center.z);
		break;
	case CollisionPlane::YZ:
		aCenter = Vector2(aBox.GetCenter().y, aBox.GetCenter().z);
		obbCenter = Vector2(obb.center.y, obb.center.z);
		break;
	}

	// OBBの2D軸ベクトルを取得
	Matrix4x4 rot = obb.rotate;
	Vector2 axes[2];
	axes[0] = Vector2(GetSizeFromIndex(Vector3(rot.m[axis1][0], rot.m[axis2][0], 0), 0),
					  GetSizeFromIndex(Vector3(rot.m[axis1][0], rot.m[axis2][0], 0), 1));
	axes[1] = Vector2(GetSizeFromIndex(Vector3(rot.m[axis1][1], rot.m[axis2][1], 0), 0),
					  GetSizeFromIndex(Vector3(rot.m[axis1][1], rot.m[axis2][1], 0), 1));

	// AABBの軸（単位軸）
	Vector2 aabbAxes[2] = { Vector2(1,0), Vector2(0,1) };
	
	// 4つの分離軸を構築（OBBの2軸 + AABBの2軸）
	Vector2 testAxes[kObb2dAxisCount] = {
		Vector2::Normalize(axes[0]),
		Vector2::Normalize(axes[1]),
		aabbAxes[0],
		aabbAxes[1]
	};

	Vector2 toCenter = aCenter - obbCenter;
	
	// AABBの半幅を平面に応じて取得
	Vector2 aHalf;
	switch (plane)
	{
	case CollisionPlane::XY:
		aHalf = Vector2(aBox.GetHalfSize().x, aBox.GetHalfSize().y);
		break;
	case CollisionPlane::XZ:
		aHalf = Vector2(aBox.GetHalfSize().x, aBox.GetHalfSize().z);
		break;
	case CollisionPlane::YZ:
		aHalf = Vector2(aBox.GetHalfSize().y, aBox.GetHalfSize().z);
		break;
	}

	// 各分離軸で判定
	for (int i = 0; i < kObb2dAxisCount; ++i)
	{
		const Vector2& axis = testAxes[i];

		// AABBの投影サイズを計算
		float aProj = std::abs(Vector2::Dot(axis, Vector2(aHalf.x, 0))) +
			std::abs(Vector2::Dot(axis, Vector2(0, aHalf.y)));

		// OBBの投影サイズを計算
		float bProj = std::abs(Vector2::Dot(axes[0] * GetSizeFromIndex(obb.size, axis1), axis)) +
			std::abs(Vector2::Dot(axes[1] * GetSizeFromIndex(obb.size, axis2), axis));

		float distance = std::abs(Vector2::Dot(toCenter, axis));

		// 分離軸が見つかった場合は衝突していない
		if (distance > aProj + bProj)
			return false;
	}

	// 衝突位置を記録
	ICollisionComponent* aNonConst = const_cast<AABBColliderComponent*>(a);
	ICollisionComponent* bNonConst = const_cast<OBBColliderComponent*>(b);
	aNonConst->SetCollisionPosition(aBox.GetCenter());
	bNonConst->SetCollisionPosition(obb.center);
	return true;
}

/**
 * @brief Circle同士の2D衝突判定
 * 
 * 判定方法: 指定平面上で中心間距離と半径の和を比較
 */
bool CollisionAlgorithm::CheckCirclevsCircle2D(const SphereColliderComponent* a, const SphereColliderComponent* b, CollisionPlane plane)
{
	// 判定平面の軸インデックスを取得
	int axis1, axis2;
	GetPlaneAxes(plane, axis1, axis2);

	const Sphere& sA = a->GetSphere();
	const Sphere& sB = b->GetSphere();

	// 2次元座標を取得
	float a1 = GetSizeFromIndex(sA.center, axis1);
	float a2 = GetSizeFromIndex(sA.center, axis2);
	float b1 = GetSizeFromIndex(sB.center, axis1);
	float b2 = GetSizeFromIndex(sB.center, axis2);

	// 中心間距離の2乗を計算
	float dx = a1 - b1;
	float dy = a2 - b2;
	float distSq = dx * dx + dy * dy;
	float radiusSum = sA.radius + sB.radius;

	// 距離 <= 半径の和 なら衝突
	if (distSq <= radiusSum * radiusSum)
	{
		// 衝突位置を記録
		ICollisionComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
		ICollisionComponent* bNonConst = const_cast<SphereColliderComponent*>(b);
		aNonConst->SetCollisionPosition(sA.center);
		bNonConst->SetCollisionPosition(sB.center);
		return true;
	}
	return false;
}

// Circle vs AABB 2D

/**
 * @brief CircleとAABBの2D衝突判定
 * 
 * 判定方法: AABB上の最近傍点との距離を判定
 */
bool CollisionAlgorithm::CheckCirclevsAABB2D(const SphereColliderComponent* a, const AABBColliderComponent* b, CollisionPlane plane)
{
	// 判定平面の軸インデックスを取得
	int axis1, axis2;
	GetPlaneAxes(plane, axis1, axis2);

	const Sphere& s = a->GetSphere();
	const AABB& box = b->GetAABB();

	// 円の中心座標を取得
	float cx = GetSizeFromIndex(s.center, axis1);
	float cy = GetSizeFromIndex(s.center, axis2);

	// AABBの境界を取得
	float minX = GetSizeFromIndex(box.min_, axis1);
	float minY = GetSizeFromIndex(box.min_, axis2);
	float maxX = GetSizeFromIndex(box.max_, axis1);
	float maxY = GetSizeFromIndex(box.max_, axis2);

	// AABB上の最近傍点を計算（各軸でクランプ）
	float closestX = (std::max)(minX, (std::min)(cx, maxX));
	float closestY = (std::max)(minY, (std::min)(cy, maxY));

	// 最近傍点との距離を計算
	float dx = cx - closestX;
	float dy = cy - closestY;
	float distSq = dx * dx + dy * dy;

	// 距離 <= 半径 なら衝突
	if (distSq <= s.radius * s.radius)
	{
		// 衝突位置を記録
		ICollisionComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
		ICollisionComponent* bNonConst = const_cast<AABBColliderComponent*>(b);
		aNonConst->SetCollisionPosition(s.center);
		// 最近傍点を3Dで返す
		Vector3 closestPt = s.center;
		bNonConst->SetCollisionPosition(closestPt);
		return true;
	}
	return false;
}

// Circle vs OBB 2D

/**
 * @brief CircleとOBBの2D衝突判定
 * 
 * 判定方法: OBB上の最近傍点との距離を判定
 */
bool CollisionAlgorithm::CheckCirclevsOBB2D(const SphereColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane)
{
	// 判定平面の軸インデックスを取得
	int axis1, axis2;
	GetPlaneAxes(plane, axis1, axis2);

	const Sphere& s = a->GetSphere();
	const OBB& obb = b->GetOBB();

	// 2D座標を取得
	float sx = GetSizeFromIndex(s.center, axis1);
	float sy = GetSizeFromIndex(s.center, axis2);
	float obb_cx = GetSizeFromIndex(obb.center, axis1);
	float obb_cy = GetSizeFromIndex(obb.center, axis2);

	// OBBの2D軸ベクトルを取得
	Vector2 axes[2];
	axes[0] = Vector2(GetSizeFromIndex(Vector3(obb.rotate.m[axis1][0], obb.rotate.m[axis2][0], 0), 0),
					  GetSizeFromIndex(Vector3(obb.rotate.m[axis1][0], obb.rotate.m[axis2][0], 0), 1));
	axes[1] = Vector2(GetSizeFromIndex(Vector3(obb.rotate.m[axis1][1], obb.rotate.m[axis2][1], 0), 0),
					  GetSizeFromIndex(Vector3(obb.rotate.m[axis1][1], obb.rotate.m[axis2][1], 0), 1));

	Vector2 obbCenter(obb_cx, obb_cy);
	Vector2 circleCenter(sx, sy);
	Vector2 d = circleCenter - obbCenter;
	Vector2 closest = obbCenter;

	// OBBの半幅を取得
	const float size1 = GetSizeFromIndex(obb.size, axis1);
	const float size2 = GetSizeFromIndex(obb.size, axis2);

	// 各軸ごとに最近傍点を算出（投影・クランプ）
	float dist1 = Vector2::Dot(d, axes[0]);
	float clamped1 = (std::max)(-size1, (std::min)(dist1, size1));
	closest += axes[0] * clamped1;

	float dist2 = Vector2::Dot(d, axes[1]);
	float clamped2 = (std::max)(-size2, (std::min)(dist2, size2));
	closest += axes[1] * clamped2;

	// 最近傍点との距離を計算
	Vector2 diff = circleCenter - closest;
	float distSq = diff.x * diff.x + diff.y * diff.y;

	// 距離 <= 半径 なら衝突
	if (distSq <= s.radius * s.radius)
	{
		// 衝突位置を記録
		ICollisionComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
		ICollisionComponent* bNonConst = const_cast<OBBColliderComponent*>(b);
		aNonConst->SetCollisionPosition(s.center);

		// 最近傍点を3Dで返す
		Vector3 closestPt = s.center;
		bNonConst->SetCollisionPosition(closestPt);
		return true;
	}
	return false;
}


// --- サブステップ 2D判定 ---

/**
 * @brief AABB同士のサブステップ2D衝突判定
 * 
 * 判定方法: 前フレーム位置から現在位置までを線分補間して判定
 */
bool CollisionAlgorithm::CheckAABBvsAABBSubstep2D(const AABBColliderComponent* a, const AABBColliderComponent* b, CollisionPlane plane)
{
	// 前フレームと現在フレームの位置を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const AABB& aBox = a->GetAABB();
	const AABB& bBox = b->GetAABB();

	// まず現在位置での静的判定を試行
	if (CheckAABBvsAABB2D(a, b, plane)) return true;

	// 移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	float maxDistance = (std::max)(distanceA, distanceB);
	// 移動距離に応じてサブステップ数を決定
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	AABBColliderComponent* aNonConst = const_cast<AABBColliderComponent*>(a);
	AABBColliderComponent* bNonConst = const_cast<AABBColliderComponent*>(b);

	AABBColliderComponent tempA(nullptr);
	AABBColliderComponent tempB(nullptr);

	// 各サブステップで判定
	for (int step = 0; step <= subStepCount; ++step)
	{
		// 補間係数t
		float t = static_cast<float>(step) / subStepCount;
		
		// 補間位置を計算
		Vector3 subPosA = MathUtils::Lerp(startA, endA, t);
		Vector3 subPosB = MathUtils::Lerp(startB, endB, t);

		// 補間位置でAABBを構築
		AABB movedAABB_A(subPosA - aBox.GetHalfSize(), subPosA + aBox.GetHalfSize());
		AABB movedAABB_B(subPosB - bBox.GetHalfSize(), subPosB + bBox.GetHalfSize());


		tempA.SetAABB(movedAABB_A);
		tempB.SetAABB(movedAABB_B);

		// 衝突判定
		if (CheckAABBvsAABB2D(&tempA, &tempB, plane))
		{
			// 衝突位置を記録
			aNonConst->SetCollisionPosition(subPosA);
			bNonConst->SetCollisionPosition(subPosB);
			return true;
		}
	}
	return false;
}

/**
 * @brief OBB同士のサブステップ2D衝突判定
 * 
 * 判定方法: 前フレーム位置から現在位置までを線分補間して判定
 */
bool CollisionAlgorithm::CheckOBBvsOBBSubstep2D(const OBBColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane)
{
	// 前フレームと現在フレームの位置を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	OBB aObb = a->GetOBB();
	OBB bObb = b->GetOBB();

	// 移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	float maxDistance = (std::max)(distanceA, distanceB);
	// 移動距離に応じてサブステップ数を決定
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	OBBColliderComponent* aNonConst = const_cast<OBBColliderComponent*>(a);
	OBBColliderComponent* bNonConst = const_cast<OBBColliderComponent*>(b);

	OBBColliderComponent tempA(nullptr);
	OBBColliderComponent tempB(nullptr);

	// 各サブステップで判定
	for (int step = 0; step < subStepCount; ++step)
	{
		// 補間係数t
		float t = static_cast<float>(step + 1) / subStepCount;
		
		// 補間位置を計算
		Vector3 subPosA = startA + (endA - startA) * t;
		Vector3 subPosB = startB + (endB - startB) * t;

		// 補間位置でOBBを構築
		OBB movedOBB_A = aObb;
		OBB movedOBB_B = bObb;
		movedOBB_A.center = subPosA;
		movedOBB_B.center = subPosB;


		tempA.SetOBB(movedOBB_A);
		tempB.SetOBB(movedOBB_B);

		// 衝突判定
		if (CheckOBBvsOBB2D(&tempA, &tempB, plane))
		{
			// 衝突位置を記録
			aNonConst->SetCollisionPosition(subPosA);
			bNonConst->SetCollisionPosition(subPosB);
			return true;
		}
	}
	return false;
}

/**
 * @brief AABBとOBBのサブステップ2D衝突判定
 * 
 * 判定方法: 前フレーム位置から現在位置までを線分補間して判定
 */
bool CollisionAlgorithm::CheckAABBvsOBBSubstep2D(const AABBColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane)
{
	// 前フレームと現在フレームの位置を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const AABB& aBox = a->GetAABB();
	OBB bObb = b->GetOBB();

	// 移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	float maxDistance = (std::max)(distanceA, distanceB);
	// 移動距離に応じてサブステップ数を決定
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	AABBColliderComponent* aNonConst = const_cast<AABBColliderComponent*>(a);
	OBBColliderComponent* bNonConst = const_cast<OBBColliderComponent*>(b);

	AABBColliderComponent tempA(nullptr);
	OBBColliderComponent tempB(nullptr);

	// 各サブステップで判定
	for (int step = 0; step < subStepCount; ++step)
	{
		// 補間係数t
		float t = static_cast<float>(step + 1) / subStepCount;
		
		// 補間位置を計算
		Vector3 subPosA = startA + (endA - startA) * t;
		Vector3 subPosB = startB + (endB - startB) * t;

		// 補間位置でコライダーを構築
		OBB movedOBB = bObb;
		movedOBB.center = subPosB;

		Vector3 aHalf = aBox.GetHalfSize();
		AABB movedAABB(subPosA - aHalf, subPosA + aHalf);


		tempA.SetAABB(movedAABB);
		tempB.SetOBB(movedOBB);

		// 衝突判定
		if (CheckAABBvsOBB2D(&tempA, &tempB, plane))
		{
			// 衝突位置を記録
			aNonConst->SetCollisionPosition(subPosA);
			bNonConst->SetCollisionPosition(subPosB);
			return true;
		}
	}
	return false;
}

// Circle vs Circle 2D サブステップ

/**
 * @brief Circle同士のサブステップ2D衝突判定
 * 
 * 判定方法: 前フレーム位置から現在位置までを線分補間して判定
 */
bool CollisionAlgorithm::CheckCirclevsCircleSubstep2D(const SphereColliderComponent* a, const SphereColliderComponent* b, CollisionPlane plane)
{
	// 前フレームと現在フレームの位置を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const Sphere& sphereA = a->GetSphere();
	const Sphere& sphereB = b->GetSphere();

	// まず現在位置での静的判定を試行
	if (CheckCirclevsCircle2D(a, b, plane)) return true;

	// 移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	float maxDistance = (std::max)(distanceA, distanceB);
	// 移動距離に応じてサブステップ数を決定
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	SphereColliderComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
	SphereColliderComponent* bNonConst = const_cast<SphereColliderComponent*>(b);

	// 判定平面の軸インデックスを取得
	int axis1, axis2;
	GetPlaneAxes(plane, axis1, axis2);

	// 各サブステップで判定
	for (int step = 1; step <= subStepCount; ++step)
	{
		// 補間係数t
		float t = static_cast<float>(step) / subStepCount;
		
		// 補間位置を計算
		Vector3 subPosA = MathUtils::Lerp(startA, endA, t);
		Vector3 subPosB = MathUtils::Lerp(startB, endB, t);

		// 2D座標を取得
		float a1 = GetSizeFromIndex(subPosA, axis1);
		float a2 = GetSizeFromIndex(subPosA, axis2);
		float b1 = GetSizeFromIndex(subPosB, axis1);
		float b2 = GetSizeFromIndex(subPosB, axis2);

		// 中心間距離を計算
		float dx = a1 - b1;
		float dy = a2 - b2;
		float distSq = dx * dx + dy * dy;
		float radiusSum = sphereA.radius + sphereB.radius;

		// 距離 <= 半径の和 なら衝突
		if (distSq <= radiusSum * radiusSum)
		{
			// 衝突位置を記録
			aNonConst->SetCollisionPosition(subPosA);
			bNonConst->SetCollisionPosition(subPosB);
			return true;
		}
	}
	return false;
}

// Circle vs AABB 2D サブステップ

/**
 * @brief CircleとAABBのサブステップ2D衝突判定
 * 
 * 判定方法: 前フレーム位置から現在位置までを線分補間して判定
 */
bool CollisionAlgorithm::CheckCirclevsAABBSubstep2D(const SphereColliderComponent* a, const AABBColliderComponent* b, CollisionPlane plane)
{
	// 前フレームと現在フレームの位置を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const Sphere& sphereA = a->GetSphere();
	const AABB& boxB = b->GetAABB();

	// まず現在位置での静的判定を試行
	if (CheckCirclevsAABB2D(a, b, plane)) return true;

	// 移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	float maxDistance = (std::max)(distanceA, distanceB);
	// 移動距離に応じてサブステップ数を決定
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	SphereColliderComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
	AABBColliderComponent* bNonConst = const_cast<AABBColliderComponent*>(b);

	// 判定平面の軸インデックスを取得
	int axis1, axis2;
	GetPlaneAxes(plane, axis1, axis2);

	Vector3 boxHalf = boxB.GetHalfSize();

	// 各サブステップで判定
	for (int step = 1; step <= subStepCount; ++step)
	{
		// 補間係数t
		float t = static_cast<float>(step) / subStepCount;
		
		// 補間位置を計算
		Vector3 subPosA = MathUtils::Lerp(startA, endA, t);
		Vector3 subPosB = MathUtils::Lerp(startB, endB, t);

		// 円の中心座標を取得
		float cx = GetSizeFromIndex(subPosA, axis1);
		float cy = GetSizeFromIndex(subPosA, axis2);

		// AABBの境界を計算
		float minX = GetSizeFromIndex(subPosB - boxHalf, axis1);
		float minY = GetSizeFromIndex(subPosB - boxHalf, axis2);
		float maxX = GetSizeFromIndex(subPosB + boxHalf, axis1);
		float maxY = GetSizeFromIndex(subPosB + boxHalf, axis2);

		// AABB上の最近傍点を計算
		float closestX = (std::max)(minX, (std::min)(cx, maxX));
		float closestY = (std::max)(minY, (std::min)(cy, maxY));

		// 最近傍点との距離を計算
		float dx = cx - closestX;
		float dy = cy - closestY;
		float distSq = dx * dx + dy * dy;

		// 距離 <= 半径 なら衝突
		if (distSq <= sphereA.radius * sphereA.radius)
		{
			// 衝突位置を記録
			aNonConst->SetCollisionPosition(subPosA);
			// 最近傍点を3Dで返す
			Vector3 closestPt = subPosA;
			bNonConst->SetCollisionPosition(closestPt);
			return true;
		}
	}
	return false;
}

// Circle vs OBB 2D サブステップ

/**
 * @brief CircleとOBBのサブステップ2D衝突判定
 * 
 * 判定方法: 前フレーム位置から現在位置までを線分補間して判定
 */
bool CollisionAlgorithm::CheckCirclevsOBBSubstep2D(const SphereColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane)
{
	// 前フレームと現在フレームの位置を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const Sphere& sphereA = a->GetSphere();
	OBB obbB = b->GetOBB();

	// まず現在位置での静的判定を試行
	if (CheckCirclevsOBB2D(a, b, plane)) return true;

	// 移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	float maxDistance = (std::max)(distanceA, distanceB);
	// 移動距離に応じてサブステップ数を決定
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	SphereColliderComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
	OBBColliderComponent* bNonConst = const_cast<OBBColliderComponent*>(b);

	// 判定平面の軸インデックスを取得
	int axis1, axis2;
	GetPlaneAxes(plane, axis1, axis2);

	// 各サブステップで判定
	for (int step = 1; step <= subStepCount; ++step)
	{
		// 補間係数t
		float t = static_cast<float>(step) / subStepCount;
		
		// 補間位置を計算
		Vector3 subPosA = MathUtils::Lerp(startA, endA, t);
		Vector3 subPosB = MathUtils::Lerp(startB, endB, t);

		// 補間位置でOBBを構築
		OBB movedOBB = obbB;
		movedOBB.center = subPosB;

		// 2D座標を取得
		float sx = GetSizeFromIndex(subPosA, axis1);
		float sy = GetSizeFromIndex(subPosA, axis2);
		float obb_cx = GetSizeFromIndex(movedOBB.center, axis1);
		float obb_cy = GetSizeFromIndex(movedOBB.center, axis2);

		// OBBの2D軸ベクトルを取得
		Vector2 axes[2];
		axes[0] = Vector2(GetSizeFromIndex(Vector3(movedOBB.rotate.m[axis1][0], movedOBB.rotate.m[axis2][0], 0), 0),
						  GetSizeFromIndex(Vector3(movedOBB.rotate.m[axis1][0], movedOBB.rotate.m[axis2][0], 0), 1));
		axes[1] = Vector2(GetSizeFromIndex(Vector3(movedOBB.rotate.m[axis1][1], movedOBB.rotate.m[axis2][1], 0), 0),
						  GetSizeFromIndex(Vector3(movedOBB.rotate.m[axis1][1], movedOBB.rotate.m[axis2][1], 0), 1));

		Vector2 obbCenter(obb_cx, obb_cy);
		Vector2 circleCenter(sx, sy);
		Vector2 d = circleCenter - obbCenter;
		Vector2 closest = obbCenter;

		// OBBの半幅を取得
		const float size1 = GetSizeFromIndex(movedOBB.size, axis1);
		const float size2 = GetSizeFromIndex(movedOBB.size, axis2);

		// 各軸に沿って最近傍点を計算（投影・クランプ）
		float dist1 = Vector2::Dot(d, axes[0]);
		float clamped1 = (std::max)(-size1, (std::min)(dist1, size1));
		closest += axes[0] * clamped1;

		float dist2 = Vector2::Dot(d, axes[1]);
		float clamped2 = (std::max)(-size2, (std::min)(dist2, size2));
		closest += axes[1] * clamped2;

		// 最近傍点との距離を計算
		Vector2 diff = circleCenter - closest;
		float distSq = diff.x * diff.x + diff.y * diff.y;

		// 距離 <= 半径 なら衝突
		if (distSq <= sphereA.radius * sphereA.radius)
		{
			// 衝突位置を記録
			aNonConst->SetCollisionPosition(subPosA);
			// 最近傍点を3Dで返す
			Vector3 closestPt = subPosA;
			bNonConst->SetCollisionPosition(closestPt);
			return true;
		}
	}
	return false;
}
