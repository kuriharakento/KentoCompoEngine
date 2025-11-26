#include "CollisionAlgorithm.h"
#include <cmath>
#include <algorithm>
#include "application/GameObject/base/GameObject.h"

namespace
{
	// OBBの軸数
	constexpr int kObbAxisCount = 3;
	
	// OBB vs OBBの分離軸数（各OBBの3軸 + 外積9軸 = 15）
	constexpr int kObbSeparatingAxesCount = 15;
	
	// AABBとOBBの分離軸数（AABBの3軸 + OBBの3軸 = 6）
	constexpr int kAabbObbSeparatingAxesCount = 6;
	
	// 2D OBBの分離軸数
	constexpr int kObb2dSeparatingAxesCount = 4;
	
	// サブステップ判定の最大ステップ距離
	constexpr float kMaxStepDistance = 1.0f;
}

// --- 3D用判定 ---

bool CollisionAlgorithm::CheckAABBvsAABB3D(const AABBColliderComponent* a, const AABBColliderComponent* b)
{
	const AABB& aBox = a->GetAABB();
	const AABB& bBox = b->GetAABB();

	// 各軸で重なりをチェック（AABB同士は軸方向のみ比較）
	bool overlapX = (aBox.max_.x >= bBox.min_.x && aBox.min_.x <= bBox.max_.x);
	bool overlapY = (aBox.max_.y >= bBox.min_.y && aBox.min_.y <= bBox.max_.y);
	bool overlapZ = (aBox.max_.z >= bBox.min_.z && aBox.min_.z <= bBox.max_.z);
	
	return overlapX && overlapY && overlapZ;
}

bool CollisionAlgorithm::CheckOBBvsOBB3D(const OBBColliderComponent* a, const OBBColliderComponent* b)
{
	const OBB& obbA = a->GetOBB();
	const OBB& obbB = b->GetOBB();

	Matrix4x4 rotA = obbA.rotate;
	Matrix4x4 rotB = obbB.rotate;

	// 各OBBのワールド軸ベクトルを取得（回転行列の各行が軸方向）
	Vector3 axesA[kObbAxisCount] =
	{
		Vector3::Normalize(Vector3(rotA.m[0][0], rotA.m[0][1], rotA.m[0][2])),
		Vector3::Normalize(Vector3(rotA.m[1][0], rotA.m[1][1], rotA.m[1][2])),
		Vector3::Normalize(Vector3(rotA.m[2][0], rotA.m[2][1], rotA.m[2][2]))
	};

	Vector3 axesB[kObbAxisCount] =
	{
		Vector3::Normalize(Vector3(rotB.m[0][0], rotB.m[0][1], rotB.m[0][2])),
		Vector3::Normalize(Vector3(rotB.m[1][0], rotB.m[1][1], rotB.m[1][2])),
		Vector3::Normalize(Vector3(rotB.m[2][0], rotB.m[2][1], rotB.m[2][2]))
	};

	// 15の分離軸を構築（Aの3軸 + Bの3軸 + 外積9軸）
	Vector3 testAxes[kObbSeparatingAxesCount];
	int axisCount = 0;

	// OBB Aの3軸を追加
	for (int i = 0; i < kObbAxisCount; ++i) testAxes[axisCount++] = axesA[i];
	// OBB Bの3軸を追加
	for (int i = 0; i < kObbAxisCount; ++i) testAxes[axisCount++] = axesB[i];

	// 外積軸を追加（各軸の組み合わせ）
	for (int i = 0; i < kObbAxisCount; ++i)
	{
		for (int j = 0; j < kObbAxisCount; ++j)
		{
			testAxes[axisCount++] = Vector3::Normalize(Vector3::Cross(axesA[i], axesB[j]));
		}
	}

	// 2つのOBBの中心間ベクトル
	Vector3 toCenter = obbB.center - obbA.center;

	// 分離軸定理（SAT）: 全軸で重なりがあれば衝突
	for (int i = 0; i < kObbSeparatingAxesCount; ++i)
	{
		const Vector3& axis = testAxes[i];
		// ゼロベクトル（平行な軸の外積結果）はスキップ
		if (axis.x == 0 && axis.y == 0 && axis.z == 0) continue;

		// 各OBBの軸への投影サイズを計算（ハーフサイズ × 軸の投影）
		float aProj =
			std::abs(Vector3::Dot(axesA[0] * obbA.size.x, axis)) +
			std::abs(Vector3::Dot(axesA[1] * obbA.size.y, axis)) +
			std::abs(Vector3::Dot(axesA[2] * obbA.size.z, axis));

		float bProj =
			std::abs(Vector3::Dot(axesB[0] * obbB.size.x, axis)) +
			std::abs(Vector3::Dot(axesB[1] * obbB.size.y, axis)) +
			std::abs(Vector3::Dot(axesB[2] * obbB.size.z, axis));

		// 中心間の軸方向距離
		float distance = std::abs(Vector3::Dot(toCenter, axis));

		// 分離軸が見つかった場合は衝突していない
		if (distance > aProj + bProj)
		{
			return false;
		}
	}
	
	// 全軸で重なりがあったため衝突、衝突位置を記録
	ICollisionComponent* aNonConst = const_cast<OBBColliderComponent*>(a);
	ICollisionComponent* bNonConst = const_cast<OBBColliderComponent*>(b);
	aNonConst->SetCollisionPosition(obbA.center);
	bNonConst->SetCollisionPosition(obbB.center);

	return true;
}

bool CollisionAlgorithm::CheckAABBvsOBB3D(const AABBColliderComponent* a, const OBBColliderComponent* b)
{
	const AABB& aBox = a->GetAABB();
	const OBB& obb = b->GetOBB();

	Matrix4x4 rot = obb.rotate;

	// OBBのワールド軸ベクトルを取得
	Vector3 axes[kObbAxisCount] =
	{
		Vector3::Normalize(Vector3(rot.m[0][0], rot.m[0][1], rot.m[0][2])),
		Vector3::Normalize(Vector3(rot.m[1][0], rot.m[1][1], rot.m[1][2])),
		Vector3::Normalize(Vector3(rot.m[2][0], rot.m[2][1], rot.m[2][2]))
	};

	// AABBの中心とOBBの中心間ベクトル
	Vector3 toCenter = aBox.GetCenter() - obb.center;
	Vector3 aHalfSize = aBox.GetHalfSize();

	// 6つの分離軸（OBBの3軸 + AABBの3軸）
	Vector3 testAxes[kAabbObbSeparatingAxesCount];
	int axisCount = 0;

	// OBBの軸を追加
	for (int i = 0; i < kObbAxisCount; ++i) testAxes[axisCount++] = axes[i];
	// AABBの軸（ワールド軸）を追加
	testAxes[axisCount++] = Vector3(1, 0, 0);
	testAxes[axisCount++] = Vector3(0, 1, 0);
	testAxes[axisCount++] = Vector3(0, 0, 1);

	// 分離軸定理で判定
	for (int i = 0; i < axisCount; ++i)
	{
		const Vector3& axis = testAxes[i];

		// AABBの投影サイズ（各成分を独立して計算）
		float aProj = std::abs(Vector3::Dot(axis, Vector3(aHalfSize.x, 0.0f, 0.0f))) +
			std::abs(Vector3::Dot(axis, Vector3(0.0f, aHalfSize.y, 0.0f))) +
			std::abs(Vector3::Dot(axis, Vector3(0.0f, 0.0f, aHalfSize.z)));

		// OBBの投影サイズ
		float bProj = std::abs(Vector3::Dot(axes[0] * obb.size.x, axis)) +
			std::abs(Vector3::Dot(axes[1] * obb.size.y, axis)) +
			std::abs(Vector3::Dot(axes[2] * obb.size.z, axis));

		float distance = std::abs(Vector3::Dot(toCenter, axis));

		// 分離軸が見つかった場合は衝突していない
		if (distance > aProj + bProj)
		{
			return false;
		}
	}

	// 衝突、衝突位置を記録
	ICollisionComponent* aNonConst = const_cast<AABBColliderComponent*>(a);
	ICollisionComponent* bNonConst = const_cast<OBBColliderComponent*>(b);
	aNonConst->SetCollisionPosition(aBox.GetCenter());
	bNonConst->SetCollisionPosition(obb.center);

	return true;
}

bool CollisionAlgorithm::CheckSpherevsSphere3D(const SphereColliderComponent* a, const SphereColliderComponent* b)
{
	const Sphere& sA = a->GetSphere();
	const Sphere& sB = b->GetSphere();

	// 中心間の距離の2乗を計算
	float distSq = (sA.center - sB.center).LengthSquared();
	// 2つの球の半径の和
	float radiusSum = sA.radius + sB.radius;

	// 距離が半径の和以下なら衝突
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

bool CollisionAlgorithm::CheckSpherevsAABB3D(const SphereColliderComponent* a, const AABBColliderComponent* b)
{
	const Sphere& s = a->GetSphere();
	const AABB& box = b->GetAABB();

	// AABBの境界内で球の中心に最も近い点を計算
	Vector3 closest(
		(std::max)(box.min_.x, (std::min)(s.center.x, box.max_.x)),
		(std::max)(box.min_.y, (std::min)(s.center.y, box.max_.y)),
		(std::max)(box.min_.z, (std::min)(s.center.z, box.max_.z))
	);
	
	// 球の中心と最近傍点の距離を計算
	float distSq = (s.center - closest).LengthSquared();

	// 距離が半径以下なら衝突
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

bool CollisionAlgorithm::CheckSpherevsOBB3D(const SphereColliderComponent* a, const OBBColliderComponent* b)
{
	const Sphere& s = a->GetSphere();
	const OBB& obb = b->GetOBB();

	// 球の中心とOBBの中心間のベクトル
	Vector3 d = s.center - obb.center;
	// 最近傍点の初期値をOBBの中心に設定
	Vector3 closest = obb.center;

	// OBBの各軸のサイズ
	const float sizes[kObbAxisCount] = { obb.size.x, obb.size.y, obb.size.z };

	// OBBの各軸に対して、球の中心を投影してクランプ
	for (int i = 0; i < kObbAxisCount; ++i)
	{
		// OBBの軸ベクトル
		Vector3 axis(obb.rotate.m[i][0], obb.rotate.m[i][1], obb.rotate.m[i][2]);
		// 軸方向への投影距離
		float dist = Vector3::Dot(d, axis);
		// OBBの境界内にクランプ
		float clamped = (std::max)(-sizes[i], (std::min)(dist, sizes[i]));
		// 最近傍点を軸方向に更新
		closest += axis * clamped;
	}
	
	// 球の中心と最近傍点の距離を計算
	float distSq = (s.center - closest).LengthSquared();

	// 距離が半径以下なら衝突
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
// 高速移動する物体のすり抜けを防止するため、前フレームから現在位置までを分割して判定

bool CollisionAlgorithm::CheckAABBvsAABBSubstep3D(const AABBColliderComponent* a, const AABBColliderComponent* b)
{
	// 前フレームと現在フレームの位置を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const AABB& aBox = a->GetAABB();
	const AABB& bBox = b->GetAABB();

	// まず現在位置での判定を試行（衝突していれば早期リターン）
	if (CheckAABBvsAABB3D(a, b)) return true;

	// 各オブジェクトの移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	// 最大移動距離に基づいてサブステップ数を決定
	float maxDistance = (std::max)(distanceA, distanceB);
	// 移動距離に応じてサブステップ数を決定（すり抜け防止）
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	AABBColliderComponent* aNonConst = const_cast<AABBColliderComponent*>(a);
	AABBColliderComponent* bNonConst = const_cast<AABBColliderComponent*>(b);

	// 一時的なコライダーを作成
	AABBColliderComponent tempA(nullptr);
	AABBColliderComponent tempB(nullptr);

	// 前フレームから現在位置までを線形補間して判定
	for (int step = 0; step <= subStepCount; ++step)
	{
		// 補間係数を計算
		float t = static_cast<float>(step) / subStepCount;

		// 補間位置を計算
		Vector3 subPosA = MathUtils::Lerp(startA, endA, t);
		Vector3 subPosB = MathUtils::Lerp(startB, endB, t);

		// 補間位置でのAABBを構築
		AABB movedAABB_A(subPosA - aBox.GetHalfSize(), subPosA + aBox.GetHalfSize());
		AABB movedAABB_B(subPosB - bBox.GetHalfSize(), subPosB + bBox.GetHalfSize());


		tempA.SetAABB(movedAABB_A);
		tempB.SetAABB(movedAABB_B);

		// このステップで衝突判定
		if (CheckAABBvsAABB3D(&tempA, &tempB))
		{
			// 衝突した位置を記録
			aNonConst->SetCollisionPosition(subPosA);
			bNonConst->SetCollisionPosition(subPosB);
			return true;
		}
	}

	return false;
}

bool CollisionAlgorithm::CheckOBBvsOBBSubstep3D(const OBBColliderComponent* a, const OBBColliderComponent* b)
{
	// 前フレームと現在フレームの位置を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	OBB aObb = a->GetOBB();
	OBB bObb = b->GetOBB();

	// 各オブジェクトの移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	// 最大移動距離に基づいてサブステップ数を決定
	float maxDistance = (std::max)(distanceA, distanceB);
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	OBBColliderComponent* aNonConst = const_cast<OBBColliderComponent*>(a);
	OBBColliderComponent* bNonConst = const_cast<OBBColliderComponent*>(b);

	// 一時的なコライダーを作成
	OBBColliderComponent tempA(nullptr);
	OBBColliderComponent tempB(nullptr);

	// 前フレームから現在位置までを線形補間して判定
	for (int step = 0; step < subStepCount; ++step)
	{
		// 補間係数を計算（1から開始）
		float t = static_cast<float>(step + 1) / subStepCount;

		// 補間位置を計算
		Vector3 subPosA = startA + (endA - startA) * t;
		Vector3 subPosB = startB + (endB - startB) * t;

		// 補間位置でのOBBを構築
		OBB movedOBB_A = aObb;
		OBB movedOBB_B = bObb;
		movedOBB_A.center = subPosA;
		movedOBB_B.center = subPosB;


		tempA.SetOBB(movedOBB_A);
		tempB.SetOBB(movedOBB_B);

		// このステップで衝突判定
		if (CheckOBBvsOBB3D(&tempA, &tempB))
		{
			// 衝突した位置を記録
			aNonConst->SetCollisionPosition(subPosA);
			bNonConst->SetCollisionPosition(subPosB);
			return true;
		}
	}

	return false;
}

bool CollisionAlgorithm::CheckAABBvsOBBSubstep3D(const AABBColliderComponent* a, const OBBColliderComponent* b)
{
	// 前フレームと現在フレームの位置を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const AABB& aBox = a->GetAABB();
	OBB bObb = b->GetOBB();

	// 各オブジェクトの移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	// 最大移動距離に基づいてサブステップ数を決定
	float maxDistance = (std::max)(distanceA, distanceB);
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	AABBColliderComponent* aNonConst = const_cast<AABBColliderComponent*>(a);
	OBBColliderComponent* bNonConst = const_cast<OBBColliderComponent*>(b);

	// 一時的なコライダーを作成
	AABBColliderComponent tempA(nullptr);
	OBBColliderComponent tempB(nullptr);

	// 前フレームから現在位置までを線形補間して判定
	for (int step = 0; step < subStepCount; ++step)
	{
		// 補間係数を計算
		float t = static_cast<float>(step + 1) / subStepCount;

		// 補間位置を計算
		Vector3 subPosA = startA + (endA - startA) * t;
		Vector3 subPosB = startB + (endB - startB) * t;

		// 補間位置でのコライダーを構築
		OBB movedOBB = bObb;
		movedOBB.center = subPosB;

		Vector3 aHalf = aBox.GetHalfSize();
		AABB movedAABB(subPosA - aHalf, subPosA + aHalf);


		tempA.SetAABB(movedAABB);
		tempB.SetOBB(movedOBB);

		// このステップで衝突判定
		if (CheckAABBvsOBB3D(&tempA, &tempB))
		{
			// 衝突した位置を記録
			aNonConst->SetCollisionPosition(subPosA);
			bNonConst->SetCollisionPosition(subPosB);
			return true;
		}
	}

	return false;
}


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

	// 各オブジェクトの移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	// 最大移動距離に基づいてサブステップ数を決定
	float maxDistance = (std::max)(distanceA, distanceB);
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	SphereColliderComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
	SphereColliderComponent* bNonConst = const_cast<SphereColliderComponent*>(b);

	// 前フレームから現在位置までを線形補間して判定
	for (int step = 1; step <= subStepCount; ++step)
	{
		// 補間係数を計算
		float t = static_cast<float>(step) / subStepCount;
		// 補間位置を計算
		Vector3 subPosA = MathUtils::Lerp(startA, endA, t);
		Vector3 subPosB = MathUtils::Lerp(startB, endB, t);

		// 補間位置での球を構築
		Sphere tempA(subPosA, sphereA.radius);
		Sphere tempB(subPosB, sphereB.radius);

		// 中心間の距離を計算
		float distSq = (subPosA - subPosB).LengthSquared();
		float radiusSum = tempA.radius + tempB.radius;

		// 距離が半径の和以下なら衝突
		if (distSq <= radiusSum * radiusSum)
		{
			// 衝突した位置を記録
			aNonConst->SetCollisionPosition(subPosA);
			bNonConst->SetCollisionPosition(subPosB);
			return true;
		}
	}
	return false;
}

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

	// 各オブジェクトの移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	// 最大移動距離に基づいてサブステップ数を決定
	float maxDistance = (std::max)(distanceA, distanceB);
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	SphereColliderComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
	AABBColliderComponent* bNonConst = const_cast<AABBColliderComponent*>(b);

	// 前フレームから現在位置までを線形補間して判定
	for (int step = 1; step <= subStepCount; ++step)
	{
		// 補間係数を計算
		float t = static_cast<float>(step) / subStepCount;
		// 補間位置を計算
		Vector3 subPosA = MathUtils::Lerp(startA, endA, t);
		Vector3 subPosB = MathUtils::Lerp(startB, endB, t);

		// 補間位置での球とAABBを構築
		Sphere tempSphere(subPosA, sphereA.radius);
		Vector3 bHalf = boxB.GetHalfSize();
		AABB movedAABB(subPosB - bHalf, subPosB + bHalf);

		// AABB内での最近傍点を計算
		Vector3 closest(
			(std::max)(movedAABB.min_.x, (std::min)(tempSphere.center.x, movedAABB.max_.x)),
			(std::max)(movedAABB.min_.y, (std::min)(tempSphere.center.y, movedAABB.max_.y)),
			(std::max)(movedAABB.min_.z, (std::min)(tempSphere.center.z, movedAABB.max_.z))
		);
		float distSq = (tempSphere.center - closest).LengthSquared();

		// 距離が半径以下なら衝突
		if (distSq <= tempSphere.radius * tempSphere.radius)
		{
			// 衝突した位置を記録
			aNonConst->SetCollisionPosition(tempSphere.center);
			bNonConst->SetCollisionPosition(closest);
			return true;
		}
	}
	return false;
}

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

	// 各オブジェクトの移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	// 最大移動距離に基づいてサブステップ数を決定
	float maxDistance = (std::max)(distanceA, distanceB);
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	SphereColliderComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
	OBBColliderComponent* bNonConst = const_cast<OBBColliderComponent*>(b);

	// 前フレームから現在位置までを線形補間して判定
	for (int step = 1; step <= subStepCount; ++step)
	{
		// 補間係数を計算
		float t = static_cast<float>(step) / subStepCount;
		// 補間位置を計算
		Vector3 subPosA = MathUtils::Lerp(startA, endA, t);
		Vector3 subPosB = MathUtils::Lerp(startB, endB, t);

		// 補間位置での球とOBBを構築
		Sphere tempSphere(subPosA, sphereA.radius);
		OBB movedOBB = obbB;
		movedOBB.center = subPosB;

		// OBBローカル空間への変換（球の中心とOBBの中心間のベクトル）
		Vector3 d = tempSphere.center - movedOBB.center;
		Vector3 closest = movedOBB.center;

		// OBBの各軸に沿って最近傍点を計算
		const float sizes[kObbAxisCount] = { movedOBB.size.x, movedOBB.size.y, movedOBB.size.z };
		for (int i = 0; i < kObbAxisCount; ++i)
		{
			// OBBの軸ベクトル
			Vector3 axis(movedOBB.rotate.m[i][0], movedOBB.rotate.m[i][1], movedOBB.rotate.m[i][2]);
			// 軸方向への投影距離
			float dist = Vector3::Dot(d, axis);
			// OBBの境界内にクランプ
			float clamped = (std::max)(-sizes[i], (std::min)(dist, sizes[i]));
			// 最近傍点を軸方向に更新
			closest += axis * clamped;
		}
		float distSq = (tempSphere.center - closest).LengthSquared();

		// 距離が半径以下なら衝突
		if (distSq <= tempSphere.radius * tempSphere.radius)
		{
			// 衝突した位置を記録
			aNonConst->SetCollisionPosition(tempSphere.center);
			bNonConst->SetCollisionPosition(closest);
			return true;
		}
	}
	return false;
}

// --- 2D用判定（平面指定） ---

// 平面の軸情報を取得（平面から2D判定に使用する軸インデックスを決定）
static void GetPlaneAxes(CollisionPlane plane, int& axis1, int& axis2)
{
	switch (plane)
	{
	case CollisionPlane::XY: axis1 = 0; axis2 = 1; break; // X軸とY軸を使用
	case CollisionPlane::XZ: axis1 = 0; axis2 = 2; break; // X軸とZ軸を使用
	case CollisionPlane::YZ: axis1 = 1; axis2 = 2; break; // Y軸とZ軸を使用
	}
}

// 軸インデックスからVector3の対応する成分を取得
float GetSizeFromIndex(const Vector3& v, int axis)
{
	switch (axis)
	{
	case 0: return v.x;
	case 1: return v.y;
	case 2: return v.z;
	default: return 0.0f;
	}
}

// --- AABB vs AABB 2D判定 ---
bool CollisionAlgorithm::CheckAABBvsAABB2D(const AABBColliderComponent* a, const AABBColliderComponent* b, CollisionPlane plane)
{
	// 指定された平面の軸インデックスを取得
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

	// 2つの軸方向での重なりをチェック
	bool overlap =
		(aMax1 >= bMin1 && aMin1 <= bMax1) &&
		(aMax2 >= bMin2 && aMin2 <= bMax2);

	// 衝突している場合は衝突位置を記録
	if (overlap)
	{
		ICollisionComponent* aNonConst = const_cast<AABBColliderComponent*>(a);
		ICollisionComponent* bNonConst = const_cast<AABBColliderComponent*>(b);
		aNonConst->SetCollisionPosition(aBox.GetCenter());
		bNonConst->SetCollisionPosition(bBox.GetCenter());
	}
	return overlap;
}

// --- OBB vs OBB 2D判定 ---
bool CollisionAlgorithm::CheckOBBvsOBB2D(const OBBColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane)
{
	const OBB& obbA = a->GetOBB();
	const OBB& obbB = b->GetOBB();

	bool isColliding = false;

	// 平面に応じた専用の判定関数を呼び出し
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

	// 衝突している場合は衝突位置を記録
	if (isColliding)
	{
		ICollisionComponent* aNonConst = const_cast<OBBColliderComponent*>(a);
		ICollisionComponent* bNonConst = const_cast<OBBColliderComponent*>(b);
		aNonConst->SetCollisionPosition(obbA.center);
		bNonConst->SetCollisionPosition(obbB.center);
	}

	return isColliding;
}

// XY平面専用の高速判定（横スクロールゲームやトップビューで使用）
bool CollisionAlgorithm::CheckOBBvsOBB_XY(const OBB& obbA, const OBB& obbB)
{
	// XY平面の中心座標を2Dベクトルとして取得
	Vector2 centerA(obbA.center.x, obbA.center.y);
	Vector2 centerB(obbB.center.x, obbB.center.y);
	Vector2 toCenter = centerB - centerA;

	// XY平面の軸ベクトル（回転行列から取得、正規化済み）
	Vector2 axesA[2] = {
		Vector2(obbA.rotate.m[0][0], obbA.rotate.m[0][1]),  // X軸
		Vector2(obbA.rotate.m[1][0], obbA.rotate.m[1][1])   // Y軸
	};

	Vector2 axesB[2] = {
		Vector2(obbB.rotate.m[0][0], obbB.rotate.m[0][1]),  // X軸
		Vector2(obbB.rotate.m[1][0], obbB.rotate.m[1][1])   // Y軸
	};

	// XY平面のサイズを2Dベクトルとして取得（Vector2.x = OBB.size.x, Vector2.y = OBB.size.y）
	Vector2 sizeA(obbA.size.x, obbA.size.y);
	Vector2 sizeB(obbB.size.x, obbB.size.y);

	// 4つの分離軸でテスト（各OBBの2軸）
	Vector2 testAxes[kObb2dSeparatingAxesCount] = { axesA[0], axesA[1], axesB[0], axesB[1] };

	// 分離軸定理で判定
	for (int i = 0; i < kObb2dSeparatingAxesCount; ++i)
	{
		const Vector2& axis = testAxes[i];

		// 各OBBの投影幅を計算
		float projA = std::abs(Vector2::Dot(axesA[0] * sizeA.x, axis)) +
			std::abs(Vector2::Dot(axesA[1] * sizeA.y, axis));

		float projB = std::abs(Vector2::Dot(axesB[0] * sizeB.x, axis)) +
			std::abs(Vector2::Dot(axesB[1] * sizeB.y, axis));

		// 中心間の軸方向距離
		float distance = std::abs(Vector2::Dot(toCenter, axis));

		// 分離軸が見つかった場合は衝突していない
		if (distance > projA + projB)
		{
			return false;
		}
	}
	// 全軸で重なりがあれば衝突
	return true;
}

// XZ平面専用の高速判定（3Dゲームの地面での判定に使用）
bool CollisionAlgorithm::CheckOBBvsOBB_XZ(const OBB& obbA, const OBB& obbB)
{
	// XZ平面の中心座標を2Dベクトルとして取得
	Vector2 centerA(obbA.center.x, obbA.center.z);
	Vector2 centerB(obbB.center.x, obbB.center.z);
	Vector2 toCenter = centerB - centerA;

	// XZ平面の軸ベクトル（回転行列のXZ成分を取得）
	Vector2 axesA[2] = {
		Vector2(obbA.rotate.m[0][0], obbA.rotate.m[0][2]),  // X軸のXZ成分
		Vector2(obbA.rotate.m[2][0], obbA.rotate.m[2][2])   // Z軸のXZ成分
	};

	Vector2 axesB[2] = {
		Vector2(obbB.rotate.m[0][0], obbB.rotate.m[0][2]),
		Vector2(obbB.rotate.m[2][0], obbB.rotate.m[2][2])
	};

	// XZ平面のサイズを2Dベクトルとして取得（Vector2.x = OBB.size.x, Vector2.y = OBB.size.z）
	Vector2 sizeA(obbA.size.x, obbA.size.z);
	Vector2 sizeB(obbB.size.x, obbB.size.z);

	// 4つの分離軸でテスト
	Vector2 testAxes[kObb2dSeparatingAxesCount] = { axesA[0], axesA[1], axesB[0], axesB[1] };

	// 分離軸定理で判定
	for (int i = 0; i < kObb2dSeparatingAxesCount; ++i)
	{
		const Vector2& axis = testAxes[i];

		// 各OBBの投影幅を計算（sizeA.y = obbA.size.z）
		float projA = std::abs(Vector2::Dot(axesA[0] * sizeA.x, axis)) +
			std::abs(Vector2::Dot(axesA[1] * sizeA.y, axis));

		float projB = std::abs(Vector2::Dot(axesB[0] * sizeB.x, axis)) +
			std::abs(Vector2::Dot(axesB[1] * sizeB.y, axis));

		// 中心間の軸方向距離
		float distance = std::abs(Vector2::Dot(toCenter, axis));

		// 分離軸が見つかった場合は衝突していない
		if (distance > projA + projB)
		{
			return false;
		}
	}
	// 全軸で重なりがあれば衝突
	return true;
}

// YZ平面専用の高速判定（縦スクロールゲームで使用）
bool CollisionAlgorithm::CheckOBBvsOBB_YZ(const OBB& obbA, const OBB& obbB)
{
	// YZ平面の中心座標を2Dベクトルとして取得
	Vector2 centerA(obbA.center.y, obbA.center.z);
	Vector2 centerB(obbB.center.y, obbB.center.z);
	Vector2 toCenter = centerB - centerA;

	// YZ平面の軸ベクトル（回転行列のYZ成分を取得）
	Vector2 axesA[2] = {
		Vector2(obbA.rotate.m[1][1], obbA.rotate.m[1][2]),  // Y軸のYZ成分
		Vector2(obbA.rotate.m[2][1], obbA.rotate.m[2][2])   // Z軸のYZ成分
	};

	Vector2 axesB[2] = {
		Vector2(obbB.rotate.m[1][1], obbB.rotate.m[1][2]),
		Vector2(obbB.rotate.m[2][1], obbB.rotate.m[2][2])
	};

	// YZ平面のサイズを2Dベクトルとして取得（Vector2.x = OBB.size.y, Vector2.y = OBB.size.z）
	Vector2 sizeA(obbA.size.y, obbA.size.z);
	Vector2 sizeB(obbB.size.y, obbB.size.z);

	// 4つの分離軸でテスト
	Vector2 testAxes[kObb2dSeparatingAxesCount] = { axesA[0], axesA[1], axesB[0], axesB[1] };

	// 分離軸定理で判定
	for (int i = 0; i < kObb2dSeparatingAxesCount; ++i)
	{
		const Vector2& axis = testAxes[i];

		// 各OBBの投影幅を計算
		float projA = std::abs(Vector2::Dot(axesA[0] * sizeA.x, axis)) +
			std::abs(Vector2::Dot(axesA[1] * sizeA.y, axis));

		float projB = std::abs(Vector2::Dot(axesB[0] * sizeB.x, axis)) +
			std::abs(Vector2::Dot(axesB[1] * sizeB.y, axis));

		// 中心間の軸方向距離
		float distance = std::abs(Vector2::Dot(toCenter, axis));

		// 分離軸が見つかった場合は衝突していない
		if (distance > projA + projB)
		{
			return false;
		}
	}
	// 全軸で重なりがあれば衝突
	return true;
}

// --- AABB vs OBB 2D判定 ---
bool CollisionAlgorithm::CheckAABBvsOBB2D(const AABBColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane)
{
	// 指定された平面の軸インデックスを取得
	int axis1, axis2;
	GetPlaneAxes(plane, axis1, axis2);

	const AABB& aBox = a->GetAABB();
	const OBB& obb = b->GetOBB();

	// 平面に応じた2D中心座標を取得
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

	// OBBの2D軸ベクトルを計算
	Matrix4x4 rot = obb.rotate;
	Vector2 axes[2];
	axes[0] = Vector2(GetSizeFromIndex(Vector3(rot.m[axis1][0], rot.m[axis2][0], 0), 0),
					  GetSizeFromIndex(Vector3(rot.m[axis1][0], rot.m[axis2][0], 0), 1));
	axes[1] = Vector2(GetSizeFromIndex(Vector3(rot.m[axis1][1], rot.m[axis2][1], 0), 0),
					  GetSizeFromIndex(Vector3(rot.m[axis1][1], rot.m[axis2][1], 0), 1));

	// AABBの軸（ワールド軸）
	Vector2 aabbAxes[2] = { Vector2(1,0), Vector2(0,1) };
	
	// 4つの分離軸を構築（OBBの2軸 + AABBの2軸）
	Vector2 testAxes[kObb2dSeparatingAxesCount] = {
		Vector2::Normalize(axes[0]),
		Vector2::Normalize(axes[1]),
		aabbAxes[0],
		aabbAxes[1]
	};

	// 中心間のベクトル
	Vector2 toCenter = aCenter - obbCenter;
	
	// AABBのハーフサイズを平面に応じて取得
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

	// 分離軸定理で判定
	for (int i = 0; i < kObb2dSeparatingAxesCount; ++i)
	{
		const Vector2& axis = testAxes[i];

		// AABBの投影サイズ
		float aProj = std::abs(Vector2::Dot(axis, Vector2(aHalf.x, 0))) +
			std::abs(Vector2::Dot(axis, Vector2(0, aHalf.y)));

		// OBBの投影サイズ
		float bProj = std::abs(Vector2::Dot(axes[0] * GetSizeFromIndex(obb.size, axis1), axis)) +
			std::abs(Vector2::Dot(axes[1] * GetSizeFromIndex(obb.size, axis2), axis));

		// 中心間の軸方向距離
		float distance = std::abs(Vector2::Dot(toCenter, axis));

		// 分離軸が見つかった場合は衝突していない
		if (distance > aProj + bProj)
			return false;
	}

	// 衝突、衝突位置を記録
	ICollisionComponent* aNonConst = const_cast<AABBColliderComponent*>(a);
	ICollisionComponent* bNonConst = const_cast<OBBColliderComponent*>(b);
	aNonConst->SetCollisionPosition(aBox.GetCenter());
	bNonConst->SetCollisionPosition(obb.center);
	return true;
}

bool CollisionAlgorithm::CheckCirclevsCircle2D(const SphereColliderComponent* a, const SphereColliderComponent* b, CollisionPlane plane)
{
	// 指定された平面の軸インデックスを取得
	int axis1, axis2;
	GetPlaneAxes(plane, axis1, axis2);

	const Sphere& sA = a->GetSphere();
	const Sphere& sB = b->GetSphere();

	// 2次元座標を取得
	float a1 = GetSizeFromIndex(sA.center, axis1);
	float a2 = GetSizeFromIndex(sA.center, axis2);
	float b1 = GetSizeFromIndex(sB.center, axis1);
	float b2 = GetSizeFromIndex(sB.center, axis2);

	// 中心間の2D距離を計算
	float dx = a1 - b1;
	float dy = a2 - b2;
	float distSq = dx * dx + dy * dy;
	float radiusSum = sA.radius + sB.radius;

	// 距離が半径の和以下なら衝突
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

// Circle vs AABB 2D判定
bool CollisionAlgorithm::CheckCirclevsAABB2D(const SphereColliderComponent* a, const AABBColliderComponent* b, CollisionPlane plane)
{
	// 指定された平面の軸インデックスを取得
	int axis1, axis2;
	GetPlaneAxes(plane, axis1, axis2);

	const Sphere& s = a->GetSphere();
	const AABB& box = b->GetAABB();

	// 円の中心の2D座標を取得
	float cx = GetSizeFromIndex(s.center, axis1);
	float cy = GetSizeFromIndex(s.center, axis2);

	// AABBの境界を取得
	float minX = GetSizeFromIndex(box.min_, axis1);
	float minY = GetSizeFromIndex(box.min_, axis2);
	float maxX = GetSizeFromIndex(box.max_, axis1);
	float maxY = GetSizeFromIndex(box.max_, axis2);

	// AABB内での最近傍点を計算
	float closestX = (std::max)(minX, (std::min)(cx, maxX));
	float closestY = (std::max)(minY, (std::min)(cy, maxY));

	// 円の中心と最近傍点の距離を計算
	float dx = cx - closestX;
	float dy = cy - closestY;
	float distSq = dx * dx + dy * dy;

	// 距離が半径以下なら衝突
	if (distSq <= s.radius * s.radius)
	{
		// 衝突位置を記録
		ICollisionComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
		ICollisionComponent* bNonConst = const_cast<AABBColliderComponent*>(b);
		aNonConst->SetCollisionPosition(s.center);
		// 3D空間での衝突位置として球の中心を設定
		Vector3 closestPt = s.center;
		bNonConst->SetCollisionPosition(closestPt);
		return true;
	}
	return false;
}

// Circle vs OBB 2D判定
bool CollisionAlgorithm::CheckCirclevsOBB2D(const SphereColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane)
{
	// 指定された平面の軸インデックスを取得
	int axis1, axis2;
	GetPlaneAxes(plane, axis1, axis2);

	const Sphere& s = a->GetSphere();
	const OBB& obb = b->GetOBB();

	// 2D座標を取得
	float sx = GetSizeFromIndex(s.center, axis1);
	float sy = GetSizeFromIndex(s.center, axis2);
	float obb_cx = GetSizeFromIndex(obb.center, axis1);
	float obb_cy = GetSizeFromIndex(obb.center, axis2);

	// OBBの2D軸ベクトルを計算
	Vector2 axes[2];
	axes[0] = Vector2(GetSizeFromIndex(Vector3(obb.rotate.m[axis1][0], obb.rotate.m[axis2][0], 0), 0),
					  GetSizeFromIndex(Vector3(obb.rotate.m[axis1][0], obb.rotate.m[axis2][0], 0), 1));
	axes[1] = Vector2(GetSizeFromIndex(Vector3(obb.rotate.m[axis1][1], obb.rotate.m[axis2][1], 0), 0),
					  GetSizeFromIndex(Vector3(obb.rotate.m[axis1][1], obb.rotate.m[axis2][1], 0), 1));

	// 2D中心座標
	Vector2 obbCenter(obb_cx, obb_cy);
	Vector2 circleCenter(sx, sy);
	Vector2 d = circleCenter - obbCenter;
	Vector2 closest = obbCenter;

	// OBBの各軸のサイズ
	const float size1 = GetSizeFromIndex(obb.size, axis1);
	const float size2 = GetSizeFromIndex(obb.size, axis2);

	// 各軸ごとに最近傍点を算出
	float dist1 = Vector2::Dot(d, axes[0]);
	float clamped1 = (std::max)(-size1, (std::min)(dist1, size1));
	closest += axes[0] * clamped1;

	float dist2 = Vector2::Dot(d, axes[1]);
	float clamped2 = (std::max)(-size2, (std::min)(dist2, size2));
	closest += axes[1] * clamped2;

	// 円の中心と最近傍点の距離を計算
	Vector2 diff = circleCenter - closest;
	float distSq = diff.x * diff.x + diff.y * diff.y;

	// 距離が半径以下なら衝突
	if (distSq <= s.radius * s.radius)
	{
		// 衝突位置を記録
		ICollisionComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
		ICollisionComponent* bNonConst = const_cast<OBBColliderComponent*>(b);
		aNonConst->SetCollisionPosition(s.center);

		// 3D空間での衝突位置として球の中心を設定
		Vector3 closestPt = s.center;
		bNonConst->SetCollisionPosition(closestPt);
		return true;
	}
	return false;
}


// --- サブステップ 2D判定 ---
// 高速移動する物体のすり抜けを防止するため、前フレームから現在位置までを分割して判定

bool CollisionAlgorithm::CheckAABBvsAABBSubstep2D(const AABBColliderComponent* a, const AABBColliderComponent* b, CollisionPlane plane)
{
	// 前フレームと現在フレームの位置を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const AABB& aBox = a->GetAABB();
	const AABB& bBox = b->GetAABB();

	// まず現在位置での判定を試行
	if (CheckAABBvsAABB2D(a, b, plane)) return true;

	// 各オブジェクトの移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	// 最大移動距離に基づいてサブステップ数を決定
	float maxDistance = (std::max)(distanceA, distanceB);
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	AABBColliderComponent* aNonConst = const_cast<AABBColliderComponent*>(a);
	AABBColliderComponent* bNonConst = const_cast<AABBColliderComponent*>(b);

	// 一時的なコライダーを作成
	AABBColliderComponent tempA(nullptr);
	AABBColliderComponent tempB(nullptr);

	// 前フレームから現在位置までを線形補間して判定
	for (int step = 0; step <= subStepCount; ++step)
	{
		// 補間係数を計算
		float t = static_cast<float>(step) / subStepCount;
		// 補間位置を計算
		Vector3 subPosA = MathUtils::Lerp(startA, endA, t);
		Vector3 subPosB = MathUtils::Lerp(startB, endB, t);

		// 補間位置でのAABBを構築
		AABB movedAABB_A(subPosA - aBox.GetHalfSize(), subPosA + aBox.GetHalfSize());
		AABB movedAABB_B(subPosB - bBox.GetHalfSize(), subPosB + bBox.GetHalfSize());


		tempA.SetAABB(movedAABB_A);
		tempB.SetAABB(movedAABB_B);

		// このステップで衝突判定
		if (CheckAABBvsAABB2D(&tempA, &tempB, plane))
		{
			// 衝突した位置を記録
			aNonConst->SetCollisionPosition(subPosA);
			bNonConst->SetCollisionPosition(subPosB);
			return true;
		}
	}
	return false;
}

bool CollisionAlgorithm::CheckOBBvsOBBSubstep2D(const OBBColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane)
{
	// 前フレームと現在フレームの位置を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	OBB aObb = a->GetOBB();
	OBB bObb = b->GetOBB();

	// 各オブジェクトの移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	// 最大移動距離に基づいてサブステップ数を決定
	float maxDistance = (std::max)(distanceA, distanceB);
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	OBBColliderComponent* aNonConst = const_cast<OBBColliderComponent*>(a);
	OBBColliderComponent* bNonConst = const_cast<OBBColliderComponent*>(b);

	// 一時的なコライダーを作成
	OBBColliderComponent tempA(nullptr);
	OBBColliderComponent tempB(nullptr);

	// 前フレームから現在位置までを線形補間して判定
	for (int step = 0; step < subStepCount; ++step)
	{
		// 補間係数を計算
		float t = static_cast<float>(step + 1) / subStepCount;
		// 補間位置を計算
		Vector3 subPosA = startA + (endA - startA) * t;
		Vector3 subPosB = startB + (endB - startB) * t;

		// 補間位置でのOBBを構築
		OBB movedOBB_A = aObb;
		OBB movedOBB_B = bObb;
		movedOBB_A.center = subPosA;
		movedOBB_B.center = subPosB;


		tempA.SetOBB(movedOBB_A);
		tempB.SetOBB(movedOBB_B);

		// このステップで衝突判定
		if (CheckOBBvsOBB2D(&tempA, &tempB, plane))
		{
			// 衝突した位置を記録
			aNonConst->SetCollisionPosition(subPosA);
			bNonConst->SetCollisionPosition(subPosB);
			return true;
		}
	}
	return false;
}

bool CollisionAlgorithm::CheckAABBvsOBBSubstep2D(const AABBColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane)
{
	// 前フレームと現在フレームの位置を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const AABB& aBox = a->GetAABB();
	OBB bObb = b->GetOBB();

	// 各オブジェクトの移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	// 最大移動距離に基づいてサブステップ数を決定
	float maxDistance = (std::max)(distanceA, distanceB);
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	AABBColliderComponent* aNonConst = const_cast<AABBColliderComponent*>(a);
	OBBColliderComponent* bNonConst = const_cast<OBBColliderComponent*>(b);

	// 一時的なコライダーを作成
	AABBColliderComponent tempA(nullptr);
	OBBColliderComponent tempB(nullptr);

	// 前フレームから現在位置までを線形補間して判定
	for (int step = 0; step < subStepCount; ++step)
	{
		// 補間係数を計算
		float t = static_cast<float>(step + 1) / subStepCount;
		Vector3 subPosA = startA + (endA - startA) * t;
		Vector3 subPosB = startB + (endB - startB) * t;

		OBB movedOBB = bObb;
		movedOBB.center = subPosB;

		Vector3 aHalf = aBox.GetHalfSize();
		AABB movedAABB(subPosA - aHalf, subPosA + aHalf);


		tempA.SetAABB(movedAABB);
		tempB.SetOBB(movedOBB);

		if (CheckAABBvsOBB2D(&tempA, &tempB, plane))
		{
			aNonConst->SetCollisionPosition(subPosA);
			bNonConst->SetCollisionPosition(subPosB);
			return true;
		}
	}
	return false;
}

// Circle vs Circle 2D サブステップ判定
bool CollisionAlgorithm::CheckCirclevsCircleSubstep2D(const SphereColliderComponent* a, const SphereColliderComponent* b, CollisionPlane plane)
{
	// 前フレームと現在フレームの位置を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const Sphere& sphereA = a->GetSphere();
	const Sphere& sphereB = b->GetSphere();

	// まず現在位置での判定を試行
	if (CheckCirclevsCircle2D(a, b, plane)) return true;

	// 各オブジェクトの移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	// 最大移動距離に基づいてサブステップ数を決定
	float maxDistance = (std::max)(distanceA, distanceB);
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	SphereColliderComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
	SphereColliderComponent* bNonConst = const_cast<SphereColliderComponent*>(b);

	// 指定された平面の軸インデックスを取得
	int axis1, axis2;
	GetPlaneAxes(plane, axis1, axis2);

	// 前フレームから現在位置までを線形補間して判定
	for (int step = 1; step <= subStepCount; ++step)
	{
		// 補間係数を計算
		float t = static_cast<float>(step) / subStepCount;
		// 補間位置を計算
		Vector3 subPosA = MathUtils::Lerp(startA, endA, t);
		Vector3 subPosB = MathUtils::Lerp(startB, endB, t);

		// 2D座標を取得
		float a1 = GetSizeFromIndex(subPosA, axis1);
		float a2 = GetSizeFromIndex(subPosA, axis2);
		float b1 = GetSizeFromIndex(subPosB, axis1);
		float b2 = GetSizeFromIndex(subPosB, axis2);

		// 中心間の2D距離を計算
		float dx = a1 - b1;
		float dy = a2 - b2;
		float distSq = dx * dx + dy * dy;
		float radiusSum = sphereA.radius + sphereB.radius;

		// 距離が半径の和以下なら衝突
		if (distSq <= radiusSum * radiusSum)
		{
			// 衝突した位置を記録
			aNonConst->SetCollisionPosition(subPosA);
			bNonConst->SetCollisionPosition(subPosB);
			return true;
		}
	}
	return false;
}

// Circle vs AABB 2D サブステップ判定
bool CollisionAlgorithm::CheckCirclevsAABBSubstep2D(const SphereColliderComponent* a, const AABBColliderComponent* b, CollisionPlane plane)
{
	// 前フレームと現在フレームの位置を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const Sphere& sphereA = a->GetSphere();
	const AABB& boxB = b->GetAABB();

	// まず現在位置での判定を試行
	if (CheckCirclevsAABB2D(a, b, plane)) return true;

	// 各オブジェクトの移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	// 最大移動距離に基づいてサブステップ数を決定
	float maxDistance = (std::max)(distanceA, distanceB);
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	SphereColliderComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
	AABBColliderComponent* bNonConst = const_cast<AABBColliderComponent*>(b);

	// 指定された平面の軸インデックスを取得
	int axis1, axis2;
	GetPlaneAxes(plane, axis1, axis2);

	Vector3 boxHalf = boxB.GetHalfSize();

	// 前フレームから現在位置までを線形補間して判定
	for (int step = 1; step <= subStepCount; ++step)
	{
		// 補間係数を計算
		float t = static_cast<float>(step) / subStepCount;
		// 補間位置を計算
		Vector3 subPosA = MathUtils::Lerp(startA, endA, t);
		Vector3 subPosB = MathUtils::Lerp(startB, endB, t);

		// 円の中心の2D座標を取得
		float cx = GetSizeFromIndex(subPosA, axis1);
		float cy = GetSizeFromIndex(subPosA, axis2);

		// AABBの境界を計算
		float minX = GetSizeFromIndex(subPosB - boxHalf, axis1);
		float minY = GetSizeFromIndex(subPosB - boxHalf, axis2);
		float maxX = GetSizeFromIndex(subPosB + boxHalf, axis1);
		float maxY = GetSizeFromIndex(subPosB + boxHalf, axis2);

		// AABB内での最近傍点を計算
		float closestX = (std::max)(minX, (std::min)(cx, maxX));
		float closestY = (std::max)(minY, (std::min)(cy, maxY));

		// 円の中心と最近傍点の距離を計算
		float dx = cx - closestX;
		float dy = cy - closestY;
		float distSq = dx * dx + dy * dy;

		// 距離が半径以下なら衝突
		if (distSq <= sphereA.radius * sphereA.radius)
		{
			// 衝突した位置を記録
			aNonConst->SetCollisionPosition(subPosA);
			// 3D空間での衝突位置として円の中心を設定
			Vector3 closestPt = subPosA;
			bNonConst->SetCollisionPosition(closestPt);
			return true;
		}
	}
	return false;
}

// Circle vs OBB 2D サブステップ判定
bool CollisionAlgorithm::CheckCirclevsOBBSubstep2D(const SphereColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane)
{
	// 前フレームと現在フレームの位置を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const Sphere& sphereA = a->GetSphere();
	OBB obbB = b->GetOBB();

	// まず現在位置での判定を試行
	if (CheckCirclevsOBB2D(a, b, plane)) return true;

	// 各オブジェクトの移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	// 最大移動距離に基づいてサブステップ数を決定
	float maxDistance = (std::max)(distanceA, distanceB);
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / kMaxStepDistance)));

	SphereColliderComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
	OBBColliderComponent* bNonConst = const_cast<OBBColliderComponent*>(b);

	// 指定された平面の軸インデックスを取得
	int axis1, axis2;
	GetPlaneAxes(plane, axis1, axis2);

	// 前フレームから現在位置までを線形補間して判定
	for (int step = 1; step <= subStepCount; ++step)
	{
		// 補間係数を計算
		float t = static_cast<float>(step) / subStepCount;
		// 補間位置を計算
		Vector3 subPosA = MathUtils::Lerp(startA, endA, t);
		Vector3 subPosB = MathUtils::Lerp(startB, endB, t);

		// 補間位置でのOBBを構築
		OBB movedOBB = obbB;
		movedOBB.center = subPosB;

		// 2D座標を取得
		float sx = GetSizeFromIndex(subPosA, axis1);
		float sy = GetSizeFromIndex(subPosA, axis2);
		float obb_cx = GetSizeFromIndex(movedOBB.center, axis1);
		float obb_cy = GetSizeFromIndex(movedOBB.center, axis2);

		// OBBの2D軸ベクトルを計算
		Vector2 axes[2];
		axes[0] = Vector2(GetSizeFromIndex(Vector3(movedOBB.rotate.m[axis1][0], movedOBB.rotate.m[axis2][0], 0), 0),
						  GetSizeFromIndex(Vector3(movedOBB.rotate.m[axis1][0], movedOBB.rotate.m[axis2][0], 0), 1));
		axes[1] = Vector2(GetSizeFromIndex(Vector3(movedOBB.rotate.m[axis1][1], movedOBB.rotate.m[axis2][1], 0), 0),
						  GetSizeFromIndex(Vector3(movedOBB.rotate.m[axis1][1], movedOBB.rotate.m[axis2][1], 0), 1));

		// 2D中心座標
		Vector2 obbCenter(obb_cx, obb_cy);
		Vector2 circleCenter(sx, sy);
		Vector2 d = circleCenter - obbCenter;
		Vector2 closest = obbCenter;

		// OBBの各軸のサイズ
		const float size1 = GetSizeFromIndex(movedOBB.size, axis1);
		const float size2 = GetSizeFromIndex(movedOBB.size, axis2);

		// 各軸ごとに最近傍点を算出
		float dist1 = Vector2::Dot(d, axes[0]);
		float clamped1 = (std::max)(-size1, (std::min)(dist1, size1));
		closest += axes[0] * clamped1;

		float dist2 = Vector2::Dot(d, axes[1]);
		float clamped2 = (std::max)(-size2, (std::min)(dist2, size2));
		closest += axes[1] * clamped2;

		// 円の中心と最近傍点の距離を計算
		Vector2 diff = circleCenter - closest;
		float distSq = diff.x * diff.x + diff.y * diff.y;

		// 距離が半径以下なら衝突
		if (distSq <= sphereA.radius * sphereA.radius)
		{
			// 衝突した位置を記録
			aNonConst->SetCollisionPosition(subPosA);
			// 3D空間での衝突位置として円の中心を設定
			Vector3 closestPt = subPosA;
			bNonConst->SetCollisionPosition(closestPt);
			return true;
		}
	}
	return false;
}
