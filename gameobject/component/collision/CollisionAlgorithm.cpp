#include "CollisionAlgorithm.h"
#include <cmath>
#include <algorithm>
#include "math/Vector2.h"
#include "engine/gameobject/base/GameObject.h"
#include "engine/ecs/components/ColliderComponent.h"

namespace KCE
{
using namespace GameObjectComponent;

namespace collisionAlgorithm
{
	using GameObjectComponent::AABBColliderComponent;
	using GameObjectComponent::OBBColliderComponent;
	using GameObjectComponent::SphereColliderComponent;
	using GameObjectComponent::RayColliderComponent;
	// --- シグネチャ統一用ラッパー関数群 ---
	// 全て「BをAから遠ざける方向のMTV」を返すよう統一する

	// AABB vs ...
	static bool AABBvsAABB_W(const ecs::ColliderComponent& a, const ecs::ColliderComponent& b, Vector3* mtv) {
		return mtv ? CheckAABBvsAABBMTV(a.worldAabb_, b.worldAabb_, *mtv) : CheckAABBvsAABB(a.worldAabb_, b.worldAabb_);
	}
	static bool AABBvsSphere_W(const ecs::ColliderComponent& a, const ecs::ColliderComponent& b, Vector3* mtv) {
		if (mtv) { bool hit = CheckSpherevsAABBMTV(b.worldSphere_, a.worldAabb_, *mtv); if (hit) *mtv = -(*mtv); return hit; }
		return CheckSpherevsAABB(b.worldSphere_, a.worldAabb_);
	}
	static bool AABBvsOBB_W(const ecs::ColliderComponent& a, const ecs::ColliderComponent& b, Vector3* mtv) {
		return mtv ? CheckAABBvsOBBMTV(a.worldAabb_, b.worldObb_, *mtv) : CheckAABBvsOBB(a.worldAabb_, b.worldObb_);
	}

	// Sphere vs ...
	static bool SpherevsAABB_W(const ecs::ColliderComponent& a, const ecs::ColliderComponent& b, Vector3* mtv) {
		return mtv ? CheckSpherevsAABBMTV(a.worldSphere_, b.worldAabb_, *mtv) : CheckSpherevsAABB(a.worldSphere_, b.worldAabb_);
	}
	static bool SpherevsSphere_W(const ecs::ColliderComponent& a, const ecs::ColliderComponent& b, Vector3* mtv) {
		return mtv ? CheckSpherevsSphereMTV(a.worldSphere_, b.worldSphere_, *mtv) : CheckSpherevsSphere(a.worldSphere_, b.worldSphere_);
	}
	static bool SpherevsOBB_W(const ecs::ColliderComponent& a, const ecs::ColliderComponent& b, Vector3* mtv) {
		return mtv ? CheckSpherevsOBBMTV(a.worldSphere_, b.worldObb_, *mtv) : CheckSpherevsOBB(a.worldSphere_, b.worldObb_);
	}

	// OBB vs ...
	static bool OBBvsAABB_W(const ecs::ColliderComponent& a, const ecs::ColliderComponent& b, Vector3* mtv) {
		if (mtv) { bool hit = CheckAABBvsOBBMTV(b.worldAabb_, a.worldObb_, *mtv); if (hit) *mtv = -(*mtv); return hit; }
		return CheckAABBvsOBB(b.worldAabb_, a.worldObb_);
	}
	static bool OBBvsSphere_W(const ecs::ColliderComponent& a, const ecs::ColliderComponent& b, Vector3* mtv) {
		if (mtv) { bool hit = CheckSpherevsOBBMTV(b.worldSphere_, a.worldObb_, *mtv); if (hit) *mtv = -(*mtv); return hit; }
		return CheckSpherevsOBB(b.worldSphere_, a.worldObb_);
	}
	static bool OBBvsOBB_W(const ecs::ColliderComponent& a, const ecs::ColliderComponent& b, Vector3* mtv) {
		return mtv ? CheckOBBvsOBBMTV(a.worldObb_, b.worldObb_, *mtv) : CheckOBBvsOBB(a.worldObb_, b.worldObb_);
	}

	// Ray vs ... (MTVは非対応)
	static bool RayvsAABB_W(const ecs::ColliderComponent& a, const ecs::ColliderComponent& b, Vector3*) { return CheckRayvsAABB3D(nullptr, nullptr); } // 既存はコンポーネント版のみ。実際は更新時に数学版を呼ぶ

	// --- CCD (Substep) ラッパー ---
	static bool AABBvsAABB_CCD(const ecs::ColliderComponent& a, const ecs::ColliderComponent& b, Vector3* mtv) {
		return mtv ? CheckAABBvsAABBSubstepMTV(a.worldAabb_, a.previousPosition_, b.worldAabb_, b.previousPosition_, *mtv) 
			       : CheckAABBvsAABBSubstep(a.worldAabb_, a.previousPosition_, b.worldAabb_, b.previousPosition_);
	}
	static bool SpherevsSphere_CCD(const ecs::ColliderComponent& a, const ecs::ColliderComponent& b, Vector3* mtv) {
		// ※現状 SpherevsSphereSubstepMTV は未実装なので通常判定を呼ぶか、必要なら追加する
		return CheckSpherevsSphereSubstep(a.worldSphere_, a.previousPosition_, b.worldSphere_, b.previousPosition_);
	}
	static bool SpherevsOBB_CCD(const ecs::ColliderComponent& a, const ecs::ColliderComponent& b, Vector3* mtv) {
		return mtv ? CheckSpherevsOBBSubstepMTV(a.worldSphere_, a.previousPosition_, b.worldObb_, b.previousPosition_, *mtv)
			       : CheckSpherevsOBBSubstep(a.worldSphere_, a.previousPosition_, b.worldObb_, b.previousPosition_);
	}
	// ... (他も同様に実装)

	// --- 関数テーブルの実体 ---
	const CollisionFunc kCollisionFuncTable[4][4] = {
		{ AABBvsAABB_W,   AABBvsSphere_W, AABBvsOBB_W,   nullptr }, // AABB
		{ SpherevsAABB_W, SpherevsSphere_W, SpherevsOBB_W, nullptr }, // Sphere
		{ OBBvsAABB_W,    OBBvsSphere_W,  OBBvsOBB_W,    nullptr }, // OBB
		{ nullptr,        nullptr,        nullptr,       nullptr }  // Ray
	};

	const CollisionFunc kCCDFuncTable[4][4] = {
		{ AABBvsAABB_CCD, nullptr,          nullptr,          nullptr },
		{ nullptr,          SpherevsSphere_CCD, SpherevsOBB_CCD,  nullptr },
		{ nullptr,          nullptr,          nullptr,          nullptr },
		{ nullptr,          nullptr,          nullptr,          nullptr }
	};

	// --- 基本的な数学的衝突判定 (コンポーネント非依存) ---

	bool CheckAABBvsAABB(const AABB& a, const AABB& b)
	{
		return (a.max_.x >= b.min_.x && a.min_.x <= b.max_.x) &&
			(a.max_.y >= b.min_.y && a.min_.y <= b.max_.y) &&
			(a.max_.z >= b.min_.z && a.min_.z <= b.max_.z);
	}

	bool CheckOBBvsOBB(const OBB& a, const OBB& b)
	{
		Matrix4x4 rotA = a.rotate;
		Matrix4x4 rotB = b.rotate;

		Vector3 axesA[3] = {
			Vector3::Normalize(Vector3(rotA.m[0][0], rotA.m[0][1], rotA.m[0][2])),
			Vector3::Normalize(Vector3(rotA.m[1][0], rotA.m[1][1], rotA.m[1][2])),
			Vector3::Normalize(Vector3(rotA.m[2][0], rotA.m[2][1], rotA.m[2][2]))
		};

		Vector3 axesB[3] = {
			Vector3::Normalize(Vector3(rotB.m[0][0], rotB.m[0][1], rotB.m[0][2])),
			Vector3::Normalize(Vector3(rotB.m[1][0], rotB.m[1][1], rotB.m[1][2])),
			Vector3::Normalize(Vector3(rotB.m[2][0], rotB.m[2][1], rotB.m[2][2]))
		};

		Vector3 testAxes[15];
		int axisCount = 0;
		for (int i = 0; i < 3; ++i) testAxes[axisCount++] = axesA[i];
		for (int i = 0; i < 3; ++i) testAxes[axisCount++] = axesB[i];
		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				Vector3 cross = Vector3::Cross(axesA[i], axesB[j]);
				float lenSq = cross.LengthSquared();
				if (lenSq > 1e-6f) testAxes[axisCount++] = cross / std::sqrt(lenSq);
			}
		}

		Vector3 toCenter = b.center - a.center;
		for (int i = 0; i < 15; ++i)
		{
			const Vector3& axis = testAxes[i];
			if (axis.x == 0 && axis.y == 0 && axis.z == 0) continue;

			float aProj = std::abs(Vector3::Dot(axesA[0] * a.size.x, axis)) +
				std::abs(Vector3::Dot(axesA[1] * a.size.y, axis)) +
				std::abs(Vector3::Dot(axesA[2] * a.size.z, axis));

			float bProj = std::abs(Vector3::Dot(axesB[0] * b.size.x, axis)) +
				std::abs(Vector3::Dot(axesB[1] * b.size.y, axis)) +
				std::abs(Vector3::Dot(axesB[2] * b.size.z, axis));

			float distance = std::abs(Vector3::Dot(toCenter, axis));
			if (distance > aProj + bProj) return false;
		}
		return true;
	}

	bool CheckAABBvsOBB(const AABB& a, const OBB& b)
	{
		const Matrix4x4& rot = b.rotate;
		Vector3 axesB[3] = {
			Vector3(rot.m[0][0], rot.m[0][1], rot.m[0][2]),
			Vector3(rot.m[1][0], rot.m[1][1], rot.m[1][2]),
			Vector3(rot.m[2][0], rot.m[2][1], rot.m[2][2])
		};
		static const Vector3 axesA[3] = { {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f} };

		Vector3 toCenter = b.center - a.GetCenter();
		Vector3 aHalfSize = a.GetHalfSize();

		Vector3 testAxes[15];
		int axisCount = 0;
		for (int i = 0; i < 3; ++i) testAxes[axisCount++] = axesA[i];
		for (int i = 0; i < 3; ++i) testAxes[axisCount++] = axesB[i];
		for (int i = 0; i < 3; ++i) {
			for (int j = 0; j < 3; ++j) {
				Vector3 cross = Vector3::Cross(axesA[i], axesB[j]);
				float lenSq = cross.LengthSquared();
				if (lenSq > 1e-6f) testAxes[axisCount++] = cross / std::sqrt(lenSq);
			}
		}

		for (int i = 0; i < axisCount; ++i) {
			const Vector3& axis = testAxes[i];
			float aProj = std::abs(Vector3::Dot(axesA[0] * aHalfSize.x, axis)) +
				std::abs(Vector3::Dot(axesA[1] * aHalfSize.y, axis)) +
				std::abs(Vector3::Dot(axesA[2] * aHalfSize.z, axis));
			float bProj = std::abs(Vector3::Dot(axesB[0] * b.size.x, axis)) +
				std::abs(Vector3::Dot(axesB[1] * b.size.y, axis)) +
				std::abs(Vector3::Dot(axesB[2] * b.size.z, axis));
			float distance = std::abs(Vector3::Dot(toCenter, axis));
			if (distance > aProj + bProj) return false;
		}
		return true;
	}

	bool CheckAABBvsOBBMTV(const AABB& a, const OBB& b, Vector3& mtv)
	{
		const Matrix4x4& rot = b.rotate;
		Vector3 axesB[3] = {
			Vector3(rot.m[0][0], rot.m[0][1], rot.m[0][2]),
			Vector3(rot.m[1][0], rot.m[1][1], rot.m[1][2]),
			Vector3(rot.m[2][0], rot.m[2][1], rot.m[2][2])
		};
		static const Vector3 axesA[3] = { {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f} };

		Vector3 toCenter = b.center - a.GetCenter();
		Vector3 aHalfSize = a.GetHalfSize();

		Vector3 testAxes[15];
		int axisCount = 0;
		for (int i = 0; i < 3; ++i) testAxes[axisCount++] = axesA[i];
		for (int i = 0; i < 3; ++i) testAxes[axisCount++] = axesB[i];
		for (int i = 0; i < 3; ++i) {
			for (int j = 0; j < 3; ++j) {
				Vector3 cross = Vector3::Cross(axesA[i], axesB[j]);
				float lenSq = cross.LengthSquared();
				if (lenSq > 1e-6f) testAxes[axisCount++] = cross / std::sqrt(lenSq);
			}
		}

		float minOverlap = FLT_MAX;
		Vector3 smallestAxis;
		Vector3 aScaled[3] = { axesA[0] * aHalfSize.x, axesA[1] * aHalfSize.y, axesA[2] * aHalfSize.z };
		Vector3 bScaled[3] = { axesB[0] * b.size.x, axesB[1] * b.size.y, axesB[2] * b.size.z };

		for (int i = 0; i < axisCount; ++i) {
			const Vector3& axis = testAxes[i];
			float aProj = std::abs(Vector3::Dot(aScaled[0], axis)) + std::abs(Vector3::Dot(aScaled[1], axis)) + std::abs(Vector3::Dot(aScaled[2], axis));
			float bProj = std::abs(Vector3::Dot(bScaled[0], axis)) + std::abs(Vector3::Dot(bScaled[1], axis)) + std::abs(Vector3::Dot(bScaled[2], axis));
			float distance = std::abs(Vector3::Dot(toCenter, axis));
			float overlap = (aProj + bProj) - distance;
			if (overlap < 0) return false;
			if (overlap < minOverlap) {
				minOverlap = overlap;
				smallestAxis = axis;
			}
		}
		if (Vector3::Dot(smallestAxis, toCenter) < 0.0f) smallestAxis = -smallestAxis;
		mtv = smallestAxis * minOverlap;
		return true;
	}

	bool CheckSpherevsSphere(const Sphere& a, const Sphere& b)
	{
		float distSq = (a.center - b.center).LengthSquared();
		float radiusSum = a.radius + b.radius;
		return distSq <= radiusSum * radiusSum;
	}

	bool CheckSpherevsAABB(const Sphere& a, const AABB& b)
	{
		Vector3 closest(
			(std::max)(b.min_.x, (std::min)(a.center.x, b.max_.x)),
			(std::max)(b.min_.y, (std::min)(a.center.y, b.max_.y)),
			(std::max)(b.min_.z, (std::min)(a.center.z, b.max_.z))
		);
		float distSq = (a.center - closest).LengthSquared();
		return distSq <= a.radius * a.radius;
	}

	bool CheckSpherevsOBB(const Sphere& a, const OBB& b)
	{
		Vector3 d = a.center - b.center;
		Vector3 closest = b.center;
		const float sizes[3] = { b.size.x, b.size.y, b.size.z };

		for (int i = 0; i < 3; ++i)
		{
			Vector3 axis(b.rotate.m[i][0], b.rotate.m[i][1], b.rotate.m[i][2]);
			float dist = Vector3::Dot(d, axis);
			float clamped = (std::max)(-sizes[i], (std::min)(dist, sizes[i]));
			closest += axis * clamped;
		}
		float distSq = (a.center - closest).LengthSquared();
		return distSq <= a.radius * a.radius;
	}

	// --- MTV (最小変位ベクトル) 付き衝突判定 ---

	bool CheckAABBvsAABBMTV(const AABB& a, const AABB& b, Vector3& mtv)
	{
		// aから見たbの位置関係による重なり量
		float overlapX1 = a.max_.x - b.min_.x; // bがaの右側から食い込んでいる量
		float overlapX2 = b.max_.x - a.min_.x; // bがaの左側から食い込んでいる量
		float overlapY1 = a.max_.y - b.min_.y; // bがaの上側から食い込んでいる量
		float overlapY2 = b.max_.y - a.min_.y; // bがaの下側から食い込んでいる量
		float overlapZ1 = a.max_.z - b.min_.z; // bがaの前側から食い込んでいる量
		float overlapZ2 = b.max_.z - a.min_.z; // bがaの後ろ側から食い込んでいる量

		if (overlapX1 < 0 || overlapX2 < 0 || overlapY1 < 0 || overlapY2 < 0 || overlapZ1 < 0 || overlapZ2 < 0) return false;

		// 最小の重なり（押し出し量）を探す
		// ox, oy, oz は「bを動かす方向」
		float ox = (overlapX1 < overlapX2) ? overlapX1 : -overlapX2;
		float oy = (overlapY1 < overlapY2) ? overlapY1 : -overlapY2;
		float oz = (overlapZ1 < overlapZ2) ? overlapZ1 : -overlapZ2;

		if (std::abs(ox) <= std::abs(oy) && std::abs(ox) <= std::abs(oz)) mtv = { ox, 0, 0 };
		else if (std::abs(oy) <= std::abs(ox) && std::abs(oy) <= std::abs(oz)) mtv = { 0, oy, 0 };
		else mtv = { 0, 0, oz };

		return true;
	}

	bool CheckOBBvsOBBMTV(const OBB& a, const OBB& b, Vector3& mtv)
	{
		Matrix4x4 rotA = a.rotate;
		Matrix4x4 rotB = b.rotate;

		Vector3 axesA[3] = {
			Vector3::Normalize(Vector3(rotA.m[0][0], rotA.m[0][1], rotA.m[0][2])),
			Vector3::Normalize(Vector3(rotA.m[1][0], rotA.m[1][1], rotA.m[1][2])),
			Vector3::Normalize(Vector3(rotA.m[2][0], rotA.m[2][1], rotA.m[2][2]))
		};

		Vector3 axesB[3] = {
			Vector3::Normalize(Vector3(rotB.m[0][0], rotB.m[0][1], rotB.m[0][2])),
			Vector3::Normalize(Vector3(rotB.m[1][0], rotB.m[1][1], rotB.m[1][2])),
			Vector3::Normalize(Vector3(rotB.m[2][0], rotB.m[2][1], rotB.m[2][2]))
		};

		Vector3 testAxes[15];
		int axisCount = 0;
		for (int i = 0; i < 3; ++i) testAxes[axisCount++] = axesA[i];
		for (int i = 0; i < 3; ++i) testAxes[axisCount++] = axesB[i];
		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				Vector3 cross = Vector3::Cross(axesA[i], axesB[j]);
				if (cross.LengthSquared() > 1e-6f) testAxes[axisCount++] = Vector3::Normalize(cross);
			}
		}

		Vector3 toCenter = b.center - a.center;
		float minOverlap = FLT_MAX;
		Vector3 smallestAxis;

		for (int i = 0; i < axisCount; ++i)
		{
			const Vector3& axis = testAxes[i];
			float aProj = std::abs(Vector3::Dot(axesA[0] * a.size.x, axis)) +
				std::abs(Vector3::Dot(axesA[1] * a.size.y, axis)) +
				std::abs(Vector3::Dot(axesA[2] * a.size.z, axis));

			float bProj = std::abs(Vector3::Dot(axesB[0] * b.size.x, axis)) +
				std::abs(Vector3::Dot(axesB[1] * b.size.y, axis)) +
				std::abs(Vector3::Dot(axesB[2] * b.size.z, axis));

			float distance = std::abs(Vector3::Dot(toCenter, axis));
			float overlap = (aProj + bProj) - distance;

			if (overlap < 0) return false;
			if (overlap < minOverlap)
			{
				minOverlap = overlap;
				smallestAxis = axis;
			}
		}

		if (Vector3::Dot(smallestAxis, toCenter) < 0.0f) smallestAxis = -smallestAxis;
		mtv = smallestAxis * minOverlap;
		return true;
	}

	bool CheckSpherevsSphereMTV(const Sphere& a, const Sphere& b, Vector3& mtv)
	{
		Vector3 diff = b.center - a.center;
		float dist = diff.Length();
		float radiusSum = a.radius + b.radius;
		if (dist > radiusSum) return false;

		if (dist < 1e-6f)
		{
			mtv = Vector3(0, 1, 0) * radiusSum;
		}
		else
		{
			mtv = (diff / dist) * (radiusSum - dist);
		}
		return true;
	}

	bool CheckSpherevsAABBMTV(const Sphere& a, const AABB& b, Vector3& mtv)
	{
		Vector3 closest(
			(std::max)(b.min_.x, (std::min)(a.center.x, b.max_.x)),
			(std::max)(b.min_.y, (std::min)(a.center.y, b.max_.y)),
			(std::max)(b.min_.z, (std::min)(a.center.z, b.max_.z))
		);

		Vector3 diff = closest - a.center;
		float distSq = diff.LengthSquared();

		if (distSq > 0.0001f)
		{
			float dist = std::sqrt(distSq);
			if (dist > a.radius) return false;
			// 修正: 第2引数(b: AABB)を押し出す方向に変更
			mtv = (diff / dist) * (a.radius - dist);
			return true;
		}

		float dMin[6] = {
			a.center.x - b.min_.x,
			b.max_.x - a.center.x,
			a.center.y - b.min_.y,
			b.max_.y - a.center.y,
			a.center.z - b.min_.z,
			b.max_.z - a.center.z
		};

		float minDist = dMin[0];
		int minIndex = 0;
		for (int i = 1; i < 6; ++i)
		{
			if (dMin[i] < minDist)
			{
				minDist = dMin[i];
				minIndex = i;
			}
		}

		float pushDist = a.radius - minDist;
		switch (minIndex)
		{
		case 0: mtv = { pushDist, 0, 0 }; break;
		case 1: mtv = { -pushDist, 0, 0 }; break;
		case 2: mtv = { 0, pushDist, 0 }; break;
		case 3: mtv = { 0, -pushDist, 0 }; break;
		case 4: mtv = { 0, 0, pushDist }; break;
		case 5: mtv = { 0, 0, -pushDist }; break;
		}

		return true;
	}

	bool CheckSpherevsOBBMTV(const Sphere& a, const OBB& b, Vector3& mtv)
	{
		Vector3 d = a.center - b.center;
		Vector3 localCenter = {
			Vector3::Dot(d, Vector3(b.rotate.m[0][0], b.rotate.m[0][1], b.rotate.m[0][2])),
			Vector3::Dot(d, Vector3(b.rotate.m[1][0], b.rotate.m[1][1], b.rotate.m[1][2])),
			Vector3::Dot(d, Vector3(b.rotate.m[2][0], b.rotate.m[2][1], b.rotate.m[2][2]))
		};

		AABB localAABB(-b.size, b.size);
		Sphere localSphere(localCenter, a.radius);

		Vector3 localMTV;
		if (CheckSpherevsAABBMTV(localSphere, localAABB, localMTV))
		{
			// localMTV は既に b(AABB) を押し出す方向になっている
			mtv = {
				localMTV.x * b.rotate.m[0][0] + localMTV.y * b.rotate.m[1][0] + localMTV.z * b.rotate.m[2][0],
				localMTV.x * b.rotate.m[0][1] + localMTV.y * b.rotate.m[1][1] + localMTV.z * b.rotate.m[2][1],
				localMTV.x * b.rotate.m[0][2] + localMTV.y * b.rotate.m[1][2] + localMTV.z * b.rotate.m[2][2]
			};
			return true;
		}
		return false;
	}

	// --- Ray 判定 ---

	bool CheckRayvsAABB(const Ray& ray, const AABB& aabb, float* outT)
	{
		float tmin = 0.0f;
		float tmax = ray.length;
		for (int i = 0; i < 3; ++i)
		{
			float invD, t0, t1;
			float start = (i == 0) ? ray.start.x : (i == 1 ? ray.start.y : ray.start.z);
			float dir = (i == 0) ? ray.direction.x : (i == 1 ? ray.direction.y : ray.direction.z);
			float minBound = (i == 0) ? aabb.min_.x : (i == 1 ? aabb.min_.y : aabb.min_.z);
			float maxBound = (i == 0) ? aabb.max_.x : (i == 1 ? aabb.max_.y : aabb.max_.z);

			if (std::abs(dir) < 1e-6f)
			{
				if (start < minBound || start > maxBound) return false;
			}
			else
			{
				invD = 1.0f / dir;
				t0 = (minBound - start) * invD;
				t1 = (maxBound - start) * invD;
				if (invD < 0.0f) std::swap(t0, t1);
				tmin = (std::max)(tmin, t0);
				tmax = (std::min)(tmax, t1);
				if (tmax <= tmin) return false;
			}
		}
		if (outT) *outT = tmin;
		return (tmin <= ray.length && tmax >= 0.0f);
	}

	bool CheckRayvsOBB(const Ray& ray, const OBB& obb, float* outT)
	{
		Vector3 localStart = ray.start - obb.center;
		Matrix4x4 invRot = Inverse(obb.rotate);
		Vector3 localRayStart = MathUtils::Transform(localStart, invRot);
		Vector3 localRayDir = MathUtils::TransformNormal(ray.direction, invRot);
		localRayDir.NormalizeSelf();

		float tmin = 0.0f;
		float tmax = ray.length;
		const float sizes[3] = { obb.size.x, obb.size.y, obb.size.z };
		const float starts[3] = { localRayStart.x, localRayStart.y, localRayStart.z };
		const float dirs[3] = { localRayDir.x, localRayDir.y, localRayDir.z };

		for (int i = 0; i < 3; ++i)
		{
			if (std::abs(dirs[i]) < 1e-6f)
			{
				if (starts[i] < -sizes[i] || starts[i] > sizes[i]) return false;
			}
			else
			{
				float invD = 1.0f / dirs[i];
				float t0 = (-sizes[i] - starts[i]) * invD;
				float t1 = (sizes[i] - starts[i]) * invD;
				if (invD < 0.0f) std::swap(t0, t1);
				tmin = (std::max)(tmin, t0);
				tmax = (std::min)(tmax, t1);
				if (tmax < tmin) return false;
			}
		}
		if (outT) *outT = tmin;
		return (tmin <= ray.length && tmax >= 0.0f);
	}

	bool CheckRayvsSphere(const Ray& ray, const Sphere& sphere, float* outT)
	{
		Vector3 m = ray.start - sphere.center;
		float c = Vector3::Dot(m, m) - sphere.radius * sphere.radius;
		if (c <= 0.0f)
		{
			if (outT) *outT = 0.0f;
			return true;
		}
		float bDot = Vector3::Dot(m, ray.direction);
		if (bDot > 0.0f) return false;
		float disc = bDot * bDot - c;
		if (disc < 0.0f) return false;
		float t = -bDot - std::sqrt(disc);
		if (t >= 0.0f && t <= ray.length)
		{
			if (outT) *outT = t;
			return true;
		}
		return false;
	}

	// --- サブステップ判定 ---

	bool CheckAABBvsAABBSubstep(const AABB& a, const Vector3& prevA, const AABB& b, const Vector3& prevB)
	{
		constexpr float MAX_STEP_DISTANCE = 1.0f;
		if (CheckAABBvsAABB(a, b)) return true;
		float maxDist = (std::max)((a.GetCenter() - prevA).Length(), (b.GetCenter() - prevB).Length());
		int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDist / MAX_STEP_DISTANCE)));
		for (int step = 1; step < subStepCount; ++step)
		{
			float t = (float)step / subStepCount;
			Vector3 subPosA = MathUtils::Lerp(prevA, a.GetCenter(), t);
			Vector3 subPosB = MathUtils::Lerp(prevB, b.GetCenter(), t);
			AABB subA(subPosA - a.GetHalfSize(), subPosA + a.GetHalfSize());
			AABB subB(subPosB - b.GetHalfSize(), subPosB + b.GetHalfSize());
			if (CheckAABBvsAABB(subA, subB)) return true;
		}
		return false;
	}

	bool CheckOBBvsOBBSubstep(const OBB& a, const Vector3& prevA, const OBB& b, const Vector3& prevB)
	{
		constexpr float MAX_STEP_DISTANCE = 0.25f; // 精度向上
		if (CheckOBBvsOBB(a, b)) return true;
		float maxDist = (std::max)((a.center - prevA).Length(), (b.center - prevB).Length());
		if (maxDist < 0.001f) return false;
		int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDist / MAX_STEP_DISTANCE)));
		for (int step = 1; step <= subStepCount; ++step)
		{
			float t = (float)step / subStepCount;
			OBB subA = a; subA.center = MathUtils::Lerp(prevA, a.center, t);
			OBB subB = b; subB.center = MathUtils::Lerp(prevB, b.center, t);
			if (CheckOBBvsOBB(subA, subB)) return true;
		}
		return false;
	}

	bool CheckAABBvsOBBSubstep(const AABB& a, const Vector3& prevA, const OBB& b, const Vector3& prevB)
	{
		constexpr float MAX_STEP_DISTANCE = 1.0f;
		if (CheckAABBvsOBB(a, b)) return true;
		float maxDist = (std::max)((a.GetCenter() - prevA).Length(), (b.center - prevB).Length());
		int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDist / MAX_STEP_DISTANCE)));
		for (int step = 1; step < subStepCount; ++step)
		{
			float t = (float)step / subStepCount;
			Vector3 subPosA = MathUtils::Lerp(prevA, a.GetCenter(), t);
			Vector3 subPosB = MathUtils::Lerp(prevB, b.center, t);
			AABB subA(subPosA - a.GetHalfSize(), subPosA + a.GetHalfSize());
			OBB subB = b; subB.center = subPosB;
			if (CheckAABBvsOBB(subA, subB)) return true;
		}
		return false;
	}

	bool CheckSpherevsSphereSubstep(const Sphere& a, const Vector3& prevA, const Sphere& b, const Vector3& prevB)
	{
		constexpr float MAX_STEP_DISTANCE = 1.0f;
		if (CheckSpherevsSphere(a, b)) return true;
		float maxDist = (std::max)((a.center - prevA).Length(), (b.center - prevB).Length());
		int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDist / MAX_STEP_DISTANCE)));
		for (int step = 1; step < subStepCount; ++step)
		{
			float t = (float)step / subStepCount;
			Sphere subA(MathUtils::Lerp(prevA, a.center, t), a.radius);
			Sphere subB(MathUtils::Lerp(prevB, b.center, t), b.radius);
			if (CheckSpherevsSphere(subA, subB)) return true;
		}
		return false;
	}

	bool CheckSpherevsAABBSubstep(const Sphere& a, const Vector3& prevA, const AABB& b, const Vector3& prevB)
	{
		constexpr float MAX_STEP_DISTANCE = 1.0f;
		if (CheckSpherevsAABB(a, b)) return true;
		float maxDist = (std::max)((a.center - prevA).Length(), (b.GetCenter() - prevB).Length());
		int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDist / MAX_STEP_DISTANCE)));
		for (int step = 1; step < subStepCount; ++step)
		{
			float t = (float)step / subStepCount;
			Sphere subA(MathUtils::Lerp(prevA, a.center, t), a.radius);
			Vector3 subPosB = MathUtils::Lerp(prevB, b.GetCenter(), t);
			AABB subB(subPosB - b.GetHalfSize(), subPosB + b.GetHalfSize());
			if (CheckSpherevsAABB(subA, subB)) return true;
		}
		return false;
	}

	bool CheckSpherevsOBBSubstep(const Sphere& a, const Vector3& prevA, const OBB& b, const Vector3& prevB)
	{
		constexpr float MAX_STEP_DISTANCE = 1.0f;
		if (CheckSpherevsOBB(a, b)) return true;
		float maxDist = (std::max)((a.center - prevA).Length(), (b.center - prevB).Length());
		int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDist / MAX_STEP_DISTANCE)));
		for (int step = 1; step < subStepCount; ++step)
		{
			float t = (float)step / subStepCount;
			Sphere subA(MathUtils::Lerp(prevA, a.center, t), a.radius);
			OBB subB = b; subB.center = MathUtils::Lerp(prevB, b.center, t);
			if (CheckSpherevsOBB(subA, subB)) return true;
		}
		return false;
	}

	bool CheckAABBvsAABBSubstepMTV(const AABB& a, const Vector3& prevA, const AABB& b, const Vector3& prevB, Vector3& mtv)
	{
		float maxDist = (std::max)((a.GetCenter() - prevA).Length(), (b.GetCenter() - prevB).Length());
		if (maxDist < 0.001f)
		{
			return CheckAABBvsAABBMTV(a, b, mtv);
		}
		constexpr float MAX_STEP_DISTANCE = 1.0f;
		int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDist / MAX_STEP_DISTANCE)));
		for (int step = 0; step < subStepCount; ++step)
		{
			float t = (float)(step + 1) / subStepCount;
			Vector3 subPosA = MathUtils::Lerp(prevA, a.GetCenter(), t);
			Vector3 subPosB = MathUtils::Lerp(prevB, b.GetCenter(), t);
			AABB subA(subPosA - a.GetHalfSize(), subPosA + a.GetHalfSize());
			AABB subB(subPosB - b.GetHalfSize(), subPosB + b.GetHalfSize());
			Vector3 subMtv;
			if (CheckAABBvsAABBMTV(subA, subB, subMtv))
			{
				// サブステップ時点での相対押し出し変位
				mtv = (subPosB - b.GetCenter()) - (subPosA - a.GetCenter()) + subMtv;
				return true;
			}
		}
		return false;
	}

	bool CheckOBBvsOBBSubstepMTV(const OBB& a, const Vector3& prevA, const OBB& b, const Vector3& prevB, Vector3& mtv)
	{
		constexpr float MAX_STEP_DISTANCE = 1.0f;

		float distA = (a.center - prevA).Length();
		float distB = (b.center - prevB).Length();
		float maxDist = (std::max)(distA, distB);

		// 既存 CheckOBBvsOBBSubstep3D と同じループ構造
		// step=0〜N-1, t=(step+1)/N で最終位置を含む端から端まで検査する
		int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDist / MAX_STEP_DISTANCE)));
		for (int step = 0; step < subStepCount; ++step)
		{
			float t = (float)(step + 1) / subStepCount;
			OBB subA = a; subA.center = MathUtils::Lerp(prevA, a.center, t);
			OBB subB = b; subB.center = MathUtils::Lerp(prevB, b.center, t);
			Vector3 subMtv;
			if (CheckOBBvsOBBMTV(subA, subB, subMtv))
			{
				mtv = (subB.center - b.center) - (subA.center - a.center) + subMtv;
				return true;
			}
		}
		return false;
	}

	bool CheckSpherevsOBBSubstepMTV(const Sphere& a, const Vector3& prevA, const OBB& b, const Vector3& prevB, Vector3& mtv)
	{
		float maxDist = (std::max)((a.center - prevA).Length(), (b.center - prevB).Length());
		if (maxDist < 0.001f)
		{
			return CheckSpherevsOBBMTV(a, b, mtv);
		}
		constexpr float MAX_STEP_DISTANCE_SPHERE_OBB = 1.0f;
		int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDist / MAX_STEP_DISTANCE_SPHERE_OBB)));
		for (int step = 0; step < subStepCount; ++step)
		{
			float t = (float)(step + 1) / subStepCount;
			Sphere subA(MathUtils::Lerp(prevA, a.center, t), a.radius);
			OBB subB = b; subB.center = MathUtils::Lerp(prevB, b.center, t);
			Vector3 subMtv;
			if (CheckSpherevsOBBMTV(subA, subB, subMtv))
			{
				mtv = (subB.center - b.center) - (subA.center - a.center) + subMtv;
				return true;
			}
		}
		return false;
	}

	bool CheckSpherevsAABBSubstepMTV(const Sphere& a, const Vector3& prevA, const AABB& b, const Vector3& prevB, Vector3& mtv)
	{
		float maxDist = (std::max)((a.center - prevA).Length(), (b.GetCenter() - prevB).Length());
		if (maxDist < 0.001f)
		{
			return CheckSpherevsAABBMTV(a, b, mtv);
		}
		constexpr float MAX_STEP_DISTANCE_SPHERE_AABB = 1.0f;
		int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDist / MAX_STEP_DISTANCE_SPHERE_AABB)));
		for (int step = 0; step < subStepCount; ++step)
		{
			float t = (float)(step + 1) / subStepCount;
			Sphere subA(MathUtils::Lerp(prevA, a.center, t), a.radius);
			Vector3 subPosB = MathUtils::Lerp(prevB, b.GetCenter(), t);
			AABB subB(subPosB - b.GetHalfSize(), subPosB + b.GetHalfSize());
			Vector3 subMtv;
			if (CheckSpherevsAABBMTV(subA, subB, subMtv))
			{
				mtv = (subPosB - b.GetCenter()) - (subA.center - a.center) + subMtv;
				return true;
			}
		}
		return false;
	}

	bool CheckAABBvsOBBSubstepMTV(const AABB& a, const Vector3& prevA, const OBB& b, const Vector3& prevB, Vector3& mtv)
	{
		float maxDist = (std::max)((a.GetCenter() - prevA).Length(), (b.center - prevB).Length());
		if (maxDist < 0.001f)
		{
			return CheckAABBvsOBBMTV(a, b, mtv);
		}
		constexpr float MAX_STEP_DISTANCE_AABB_OBB = 1.0f;
		int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDist / MAX_STEP_DISTANCE_AABB_OBB)));
		for (int step = 0; step < subStepCount; ++step)
		{
			float t = (float)(step + 1) / subStepCount;
			Vector3 subPosA = MathUtils::Lerp(prevA, a.GetCenter(), t);
			AABB subA(subPosA - a.GetHalfSize(), subPosA + a.GetHalfSize());
			OBB subB = b; subB.center = MathUtils::Lerp(prevB, b.center, t);
			Vector3 subMtv;
			if (CheckAABBvsOBBMTV(subA, subB, subMtv))
			{
				mtv = (subB.center - b.center) - (subPosA - a.GetCenter()) + subMtv;
				return true;
			}
		}
		return false;
	}

// --- 3D用判定 (既存のコンポーネント版) ---
	bool CheckAABBvsAABB3D(const AABBColliderComponent* a, const AABBColliderComponent* b)
	{
		return CheckAABBvsAABB(a->GetAABB(), b->GetAABB());
	}

	bool CheckOBBvsOBB3D(const OBBColliderComponent* a, const OBBColliderComponent* b)
	{
		if (CheckOBBvsOBB(a->GetOBB(), b->GetOBB()))
		{
			const_cast<OBBColliderComponent*>(a)->SetCollisionPosition(a->GetOBB().center);
			const_cast<OBBColliderComponent*>(b)->SetCollisionPosition(b->GetOBB().center);
			return true;
		}
		return false;
	}

	bool CheckAABBvsOBB3D(const AABBColliderComponent* a, const OBBColliderComponent* b)
	{
		if (CheckAABBvsOBB(a->GetAABB(), b->GetOBB()))
		{
			const_cast<AABBColliderComponent*>(a)->SetCollisionPosition(a->GetAABB().GetCenter());
			const_cast<OBBColliderComponent*>(b)->SetCollisionPosition(b->GetOBB().center);
			return true;
		}
		return false;
	}

	bool CheckSpherevsSphere3D(const SphereColliderComponent* a, const SphereColliderComponent* b)
	{
		if (CheckSpherevsSphere(a->GetSphere(), b->GetSphere()))
		{
			const_cast<SphereColliderComponent*>(a)->SetCollisionPosition(a->GetSphere().center);
			const_cast<SphereColliderComponent*>(b)->SetCollisionPosition(b->GetSphere().center);
			return true;
		}
		return false;
	}
} // namespace collisionAlgorithm

bool collisionAlgorithm::CheckSpherevsAABB3D(const SphereColliderComponent* a, const AABBColliderComponent* b)
{
	const Sphere& s = a->GetSphere();
	const AABB& box = b->GetAABB();

	// 最近傍点計算
	Vector3 closest(
		(std::max)(box.min_.x, (std::min)(s.center.x, box.max_.x)),
		(std::max)(box.min_.y, (std::min)(s.center.y, box.max_.y)),
		(std::max)(box.min_.z, (std::min)(s.center.z, box.max_.z))
	);

	if (CheckSpherevsAABB(s, box))
	{
		ICollisionComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
		ICollisionComponent* bNonConst = const_cast<AABBColliderComponent*>(b);
		aNonConst->SetCollisionPosition(s.center);
		bNonConst->SetCollisionPosition(closest);
		return true;
	}
	return false;
}

bool collisionAlgorithm::CheckSpherevsOBB3D(const SphereColliderComponent* a, const OBBColliderComponent* b)
{
	const Sphere& s = a->GetSphere();
	const OBB& obb = b->GetOBB();

	Vector3 d = s.center - obb.center;
	Vector3 closest = obb.center;

	const float sizes[3] = { obb.size.x, obb.size.y, obb.size.z };

	// OBB上の最近点を計算
	for (int i = 0; i < 3; ++i)
	{
		Vector3 axis(obb.rotate.m[i][0], obb.rotate.m[i][1], obb.rotate.m[i][2]);
		float dist = Vector3::Dot(d, axis);
		float clamped = (std::max)(-sizes[i], (std::min)(dist, sizes[i]));
		closest += axis * clamped;
	}

	if (CheckSpherevsOBB(s, obb))
	{
		ICollisionComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
		ICollisionComponent* bNonConst = const_cast<OBBColliderComponent*>(b);
		aNonConst->SetCollisionPosition(s.center);
		bNonConst->SetCollisionPosition(closest);
		return true;
	}
	return false;
}

// --- Ray 判定 ---

bool collisionAlgorithm::CheckRayvsAABB3D(const RayColliderComponent* a, const AABBColliderComponent* b)
{
	float t;
	if (CheckRayvsAABB(a->GetRay(), b->GetAABB(), &t))
	{
		Vector3 hitPos = a->GetRay().start + a->GetRay().direction * t;
		const_cast<RayColliderComponent*>(a)->SetCollisionPosition(hitPos);
		const_cast<AABBColliderComponent*>(b)->SetCollisionPosition(hitPos);
		return true;
	}
	return false;
}

bool collisionAlgorithm::CheckRayvsOBB3D(const RayColliderComponent* a, const OBBColliderComponent* b)
{
	float t;
	if (CheckRayvsOBB(a->GetRay(), b->GetOBB(), &t))
	{
		Vector3 hitPos = a->GetRay().start + a->GetRay().direction * t;
		const_cast<RayColliderComponent*>(a)->SetCollisionPosition(hitPos);
		const_cast<OBBColliderComponent*>(b)->SetCollisionPosition(hitPos);
		return true;
	}
	return false;
}

bool collisionAlgorithm::CheckRayvsSphere3D(const RayColliderComponent* a, const SphereColliderComponent* b)
{
	float t;
	if (CheckRayvsSphere(a->GetRay(), b->GetSphere(), &t))
	{
		Vector3 hitPos = a->GetRay().start + a->GetRay().direction * t;
		const_cast<RayColliderComponent*>(a)->SetCollisionPosition(hitPos);
		const_cast<SphereColliderComponent*>(b)->SetCollisionPosition(hitPos);
		return true;
	}
	return false;
}

// --- 3Dサブステップ判定 (コンポーネント版) ---

bool collisionAlgorithm::CheckAABBvsAABBSubstep3D(const AABBColliderComponent* a, const AABBColliderComponent* b)
{
	Vector3 prevA = a->GetPreviousPosition();
	Vector3 prevB = b->GetPreviousPosition();
	if (CheckAABBvsAABBSubstep(a->GetAABB(), prevA, b->GetAABB(), prevB))
	{
		const_cast<AABBColliderComponent*>(a)->SetCollisionPosition(a->GetAABB().GetCenter());
		const_cast<AABBColliderComponent*>(b)->SetCollisionPosition(b->GetAABB().GetCenter());
		return true;
	}
	return false;
}

bool collisionAlgorithm::CheckOBBvsOBBSubstep3D(const OBBColliderComponent* a, const OBBColliderComponent* b)
{
	Vector3 prevA = a->GetPreviousPosition();
	Vector3 prevB = b->GetPreviousPosition();
	if (CheckOBBvsOBBSubstep(a->GetOBB(), prevA, b->GetOBB(), prevB))
	{
		const_cast<OBBColliderComponent*>(a)->SetCollisionPosition(a->GetOBB().center);
		const_cast<OBBColliderComponent*>(b)->SetCollisionPosition(b->GetOBB().center);
		return true;
	}
	return false;
}

bool collisionAlgorithm::CheckAABBvsOBBSubstep3D(const AABBColliderComponent* a, const OBBColliderComponent* b)
{
	Vector3 prevA = a->GetPreviousPosition();
	Vector3 prevB = b->GetPreviousPosition();
	if (CheckAABBvsOBBSubstep(a->GetAABB(), prevA, b->GetOBB(), prevB))
	{
		const_cast<AABBColliderComponent*>(a)->SetCollisionPosition(a->GetAABB().GetCenter());
		const_cast<OBBColliderComponent*>(b)->SetCollisionPosition(b->GetOBB().center);
		return true;
	}
	return false;
}

bool collisionAlgorithm::CheckSpherevsSphereSubstep3D(const SphereColliderComponent* a, const SphereColliderComponent* b)
{
	Vector3 prevA = a->GetPreviousPosition();
	Vector3 prevB = b->GetPreviousPosition();
	if (CheckSpherevsSphereSubstep(a->GetSphere(), prevA, b->GetSphere(), prevB))
	{
		const_cast<SphereColliderComponent*>(a)->SetCollisionPosition(a->GetSphere().center);
		const_cast<SphereColliderComponent*>(b)->SetCollisionPosition(b->GetSphere().center);
		return true;
	}
	return false;
}

bool collisionAlgorithm::CheckSpherevsAABBSubstep3D(const SphereColliderComponent* a, const AABBColliderComponent* b)
{
	Vector3 prevA = a->GetPreviousPosition();
	Vector3 prevB = b->GetPreviousPosition();
	if (CheckSpherevsAABBSubstep(a->GetSphere(), prevA, b->GetAABB(), prevB))
	{
		const_cast<SphereColliderComponent*>(a)->SetCollisionPosition(a->GetSphere().center);
		const_cast<AABBColliderComponent*>(b)->SetCollisionPosition(b->GetAABB().GetCenter());
		return true;
	}
	return false;
}

bool collisionAlgorithm::CheckSpherevsOBBSubstep3D(const SphereColliderComponent* a, const OBBColliderComponent* b)
{
	Vector3 prevA = a->GetPreviousPosition();
	Vector3 prevB = b->GetPreviousPosition();
	if (CheckSpherevsOBBSubstep(a->GetSphere(), prevA, b->GetOBB(), prevB))
	{
		const_cast<SphereColliderComponent*>(a)->SetCollisionPosition(a->GetSphere().center);
		const_cast<OBBColliderComponent*>(b)->SetCollisionPosition(b->GetOBB().center);
		return true;
	}
	return false;
}

// --- 2D用判定（平面指定） ---

// 平面の軸情報を取得
static void GetPlaneAxes(CollisionPlane plane, int& axis1, int& axis2)
{
	switch (plane)
	{
	case CollisionPlane::XY: axis1 = 0; axis2 = 1; break;
	case CollisionPlane::XZ: axis1 = 0; axis2 = 2; break;
	case CollisionPlane::YZ: axis1 = 1; axis2 = 2; break;
	}
}

// 軸インデックスから値取得
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
bool collisionAlgorithm::CheckAABBvsAABB2D(const AABBColliderComponent* a, const AABBColliderComponent* b, CollisionPlane plane)
{
	int axis1, axis2;
	GetPlaneAxes(plane, axis1, axis2);

	const AABB& aBox = a->GetAABB();
	const AABB& bBox = b->GetAABB();

	// 各軸のmin/max
	float aMin1 = GetSizeFromIndex(aBox.min_, axis1);
	float aMax1 = GetSizeFromIndex(aBox.max_, axis1);
	float aMin2 = GetSizeFromIndex(aBox.min_, axis2);
	float aMax2 = GetSizeFromIndex(aBox.max_, axis2);

	float bMin1 = GetSizeFromIndex(bBox.min_, axis1);
	float bMax1 = GetSizeFromIndex(bBox.max_, axis1);
	float bMin2 = GetSizeFromIndex(bBox.min_, axis2);
	float bMax2 = GetSizeFromIndex(bBox.max_, axis2);

	bool overlap =
		(aMax1 >= bMin1 && aMin1 <= bMax1) &&
		(aMax2 >= bMin2 && aMin2 <= bMax2);

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
bool collisionAlgorithm::CheckOBBvsOBB2D(const OBBColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane)
{
	const OBB& obbA = a->GetOBB();
	const OBB& obbB = b->GetOBB();

	bool isColliding = false;

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
		// 衝突位置の設定
		ICollisionComponent* aNonConst = const_cast<OBBColliderComponent*>(a);
		ICollisionComponent* bNonConst = const_cast<OBBColliderComponent*>(b);
		aNonConst->SetCollisionPosition(obbA.center);
		bNonConst->SetCollisionPosition(obbB.center);
	}

	return isColliding;
}

// XY平面専用の高速判定
bool collisionAlgorithm::CheckOBBvsOBB_XY(const OBB& obbA, const OBB& obbB)
{
	// XY平面の中心座標（直接アクセス）
	KCE::Vector2 centerA(obbA.center.x, obbA.center.y);
	KCE::Vector2 centerB(obbB.center.x, obbB.center.y);
	KCE::Vector2 toCenter = centerB - centerA;

	// XY平面の軸ベクトル（回転行列から直接取得、正規化済み）
	KCE::Vector2 axesA[2] = {
		KCE::Vector2(obbA.rotate.m[0][0], obbA.rotate.m[0][1]),  // X軸
		KCE::Vector2(obbA.rotate.m[1][0], obbA.rotate.m[1][1])   // Y軸
	};

	KCE::Vector2 axesB[2] = {
		KCE::Vector2(obbB.rotate.m[0][0], obbB.rotate.m[0][1]),  // X軸
		KCE::Vector2(obbB.rotate.m[1][0], obbB.rotate.m[1][1])   // Y軸
	};

	// XY平面のサイズ
	KCE::Vector2 sizeA(obbA.size.x, obbA.size.y);
	KCE::Vector2 sizeB(obbB.size.x, obbB.size.y);

	// 4つの分離軸でテスト
	KCE::Vector2 testAxes[4] = { axesA[0], axesA[1], axesB[0], axesB[1] };

	for (int i = 0; i < 4; ++i)
	{
		const KCE::Vector2& axis = testAxes[i];

		// 投影幅計算
		float projA = std::abs(KCE::Vector2::Dot(axesA[0] * sizeA.x, axis)) +
			std::abs(KCE::Vector2::Dot(axesA[1] * sizeA.y, axis));

		float projB = std::abs(KCE::Vector2::Dot(axesB[0] * sizeB.x, axis)) +
			std::abs(KCE::Vector2::Dot(axesB[1] * sizeB.y, axis));

		float distance = std::abs(KCE::Vector2::Dot(toCenter, axis));

		if (distance > projA + projB)
		{
			return false; // 分離軸発見
		}
	}
	return true; // 衝突
}

// XZ平面専用の高速判定
bool collisionAlgorithm::CheckOBBvsOBB_XZ(const OBB& obbA, const OBB& obbB)
{
	KCE::Vector2 centerA(obbA.center.x, obbA.center.z);
	KCE::Vector2 centerB(obbB.center.x, obbB.center.z);
	KCE::Vector2 toCenter = centerB - centerA;

	KCE::Vector2 axesA[2] = {
		KCE::Vector2(obbA.rotate.m[0][0], obbA.rotate.m[0][2]),  // X軸のXZ成分
		KCE::Vector2(obbA.rotate.m[2][0], obbA.rotate.m[2][2])   // Z軸のXZ成分
	};

	KCE::Vector2 axesB[2] = {
		KCE::Vector2(obbB.rotate.m[0][0], obbB.rotate.m[0][2]),
		KCE::Vector2(obbB.rotate.m[2][0], obbB.rotate.m[2][2])
	};

	KCE::Vector2 sizeA(obbA.size.x, obbA.size.z);  // XZなのでZ成分
	KCE::Vector2 sizeB(obbB.size.x, obbB.size.z);  // XZなのでZ成分

	KCE::Vector2 testAxes[4] = { axesA[0], axesA[1], axesB[0], axesB[1] };

	for (int i = 0; i < 4; ++i)
	{
		const KCE::Vector2& axis = testAxes[i];

		// 修正: sizeA.x と sizeA.y ではなく、sizeA.x と sizeA.z
		float projA = std::abs(KCE::Vector2::Dot(axesA[0] * sizeA.x, axis)) +
			std::abs(KCE::Vector2::Dot(axesA[1] * sizeA.y, axis));  // ← ここをsizeA.yに修正

		float projB = std::abs(KCE::Vector2::Dot(axesB[0] * sizeB.x, axis)) +
			std::abs(KCE::Vector2::Dot(axesB[1] * sizeB.y, axis));  // ← ここをsizeB.yに修正

		float distance = std::abs(KCE::Vector2::Dot(toCenter, axis));

		if (distance > projA + projB)
		{
			return false;
		}
	}
	return true;
}

// YZ平面専用の高速判定
bool collisionAlgorithm::CheckOBBvsOBB_YZ(const OBB& obbA, const OBB& obbB)
{
	KCE::Vector2 centerA(obbA.center.y, obbA.center.z);
	KCE::Vector2 centerB(obbB.center.y, obbB.center.z);
	KCE::Vector2 toCenter = centerB - centerA;

	KCE::Vector2 axesA[2] = {
		KCE::Vector2(obbA.rotate.m[1][1], obbA.rotate.m[1][2]),  // Y軸のYZ成分
		KCE::Vector2(obbA.rotate.m[2][1], obbA.rotate.m[2][2])   // Z軸のYZ成分
	};

	KCE::Vector2 axesB[2] = {
		KCE::Vector2(obbB.rotate.m[1][1], obbB.rotate.m[1][2]),
		KCE::Vector2(obbB.rotate.m[2][1], obbB.rotate.m[2][2])
	};

	KCE::Vector2 sizeA(obbA.size.y, obbA.size.z);  // YZなのでY,Z成分
	KCE::Vector2 sizeB(obbB.size.y, obbB.size.z);  // YZなのでY,Z成分

	KCE::Vector2 testAxes[4] = { axesA[0], axesA[1], axesB[0], axesB[1] };

	for (int i = 0; i < 4; ++i)
	{
		const KCE::Vector2& axis = testAxes[i];

		// 修正: YZ平面なので sizeA.x と sizeA.y を使用
		float projA = std::abs(KCE::Vector2::Dot(axesA[0] * sizeA.x, axis)) +  // sizeA.x = obbA.size.y
			std::abs(KCE::Vector2::Dot(axesA[1] * sizeA.y, axis));   // sizeA.y = obbA.size.z

		float projB = std::abs(KCE::Vector2::Dot(axesB[0] * sizeB.x, axis)) +
			std::abs(KCE::Vector2::Dot(axesB[1] * sizeB.y, axis));

		float distance = std::abs(KCE::Vector2::Dot(toCenter, axis));

		if (distance > projA + projB)
		{
			return false;
		}
	}
	return true;
}

// --- AABB vs OBB 2D判定 ---
bool collisionAlgorithm::CheckAABBvsOBB2D(const AABBColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane)
{
	int axis1, axis2;
	GetPlaneAxes(plane, axis1, axis2);

	const AABB& aBox = a->GetAABB();
	const OBB& obb = b->GetOBB();

	KCE::Vector2 aCenter, obbCenter;
	switch (plane)
	{
	case CollisionPlane::XY:
		aCenter = KCE::Vector2(aBox.GetCenter().x, aBox.GetCenter().y);
		obbCenter = KCE::Vector2(obb.center.x, obb.center.y);
		break;
	case CollisionPlane::XZ:
		aCenter = KCE::Vector2(aBox.GetCenter().x, aBox.GetCenter().z);
		obbCenter = KCE::Vector2(obb.center.x, obb.center.z);
		break;
	case CollisionPlane::YZ:
		aCenter = KCE::Vector2(aBox.GetCenter().y, aBox.GetCenter().z);
		obbCenter = KCE::Vector2(obb.center.y, obb.center.z);
		break;
	}

	Matrix4x4 rot = obb.rotate;
	KCE::Vector2 axes[2];
	axes[0] = KCE::Vector2(GetSizeFromIndex(Vector3(rot.m[axis1][0], rot.m[axis2][0], 0), 0),
					  GetSizeFromIndex(Vector3(rot.m[axis1][0], rot.m[axis2][0], 0), 1));
	axes[1] = KCE::Vector2(GetSizeFromIndex(Vector3(rot.m[axis1][1], rot.m[axis2][1], 0), 0),
					  GetSizeFromIndex(Vector3(rot.m[axis1][1], rot.m[axis2][1], 0), 1));

	KCE::Vector2 aabbAxes[2] = { KCE::Vector2(1,0), KCE::Vector2(0,1) };
	KCE::Vector2 testAxes[4] = {
		KCE::Vector2::Normalize(axes[0]),
		KCE::Vector2::Normalize(axes[1]),
		aabbAxes[0],
		aabbAxes[1]
	};

	KCE::Vector2 toCenter = aCenter - obbCenter;
	KCE::Vector2 aHalf;
	switch (plane)
	{
	case CollisionPlane::XY:
		aHalf = KCE::Vector2(aBox.GetHalfSize().x, aBox.GetHalfSize().y);
		break;
	case CollisionPlane::XZ:
		aHalf = KCE::Vector2(aBox.GetHalfSize().x, aBox.GetHalfSize().z);
		break;
	case CollisionPlane::YZ:
		aHalf = KCE::Vector2(aBox.GetHalfSize().y, aBox.GetHalfSize().z);
		break;
	}

	for (int i = 0; i < 4; ++i)
	{
		const KCE::Vector2& axis = testAxes[i];

		float aProj = std::abs(KCE::Vector2::Dot(axis, KCE::Vector2(aHalf.x, 0))) +
			std::abs(KCE::Vector2::Dot(axis, KCE::Vector2(0, aHalf.y)));

		float bProj = std::abs(KCE::Vector2::Dot(axes[0] * GetSizeFromIndex(obb.size, axis1), axis)) +
			std::abs(KCE::Vector2::Dot(axes[1] * GetSizeFromIndex(obb.size, axis2), axis));

		float distance = std::abs(KCE::Vector2::Dot(toCenter, axis));

		if (distance > aProj + bProj)
			return false;
	}

	ICollisionComponent* aNonConst = const_cast<AABBColliderComponent*>(a);
	ICollisionComponent* bNonConst = const_cast<OBBColliderComponent*>(b);
	aNonConst->SetCollisionPosition(aBox.GetCenter());
	bNonConst->SetCollisionPosition(obb.center);
	return true;
}

bool collisionAlgorithm::CheckCirclevsCircle2D(const SphereColliderComponent* a, const SphereColliderComponent* b, CollisionPlane plane)
{
	int axis1, axis2;
	GetPlaneAxes(plane, axis1, axis2);

	const Sphere& sA = a->GetSphere();
	const Sphere& sB = b->GetSphere();

	// 2次元ベクトル化
	float a1 = GetSizeFromIndex(sA.center, axis1);
	float a2 = GetSizeFromIndex(sA.center, axis2);
	float b1 = GetSizeFromIndex(sB.center, axis1);
	float b2 = GetSizeFromIndex(sB.center, axis2);

	float dx = a1 - b1;
	float dy = a2 - b2;
	float distSq = dx * dx + dy * dy;
	float radiusSum = sA.radius + sB.radius;

	if (distSq <= radiusSum * radiusSum)
	{
		ICollisionComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
		ICollisionComponent* bNonConst = const_cast<SphereColliderComponent*>(b);
		aNonConst->SetCollisionPosition(sA.center);
		bNonConst->SetCollisionPosition(sB.center);
		return true;
	}
	return false;
}

// Circle vs AABB 2D
bool collisionAlgorithm::CheckCirclevsAABB2D(const SphereColliderComponent* a, const AABBColliderComponent* b, CollisionPlane plane)
{
	int axis1, axis2;
	GetPlaneAxes(plane, axis1, axis2);

	const Sphere& s = a->GetSphere();
	const AABB& box = b->GetAABB();

	float cx = GetSizeFromIndex(s.center, axis1);
	float cy = GetSizeFromIndex(s.center, axis2);

	float minX = GetSizeFromIndex(box.min_, axis1);
	float minY = GetSizeFromIndex(box.min_, axis2);
	float maxX = GetSizeFromIndex(box.max_, axis1);
	float maxY = GetSizeFromIndex(box.max_, axis2);

	// 最近傍点
	float closestX = (std::max)(minX, (std::min)(cx, maxX));
	float closestY = (std::max)(minY, (std::min)(cy, maxY));

	float dx = cx - closestX;
	float dy = cy - closestY;
	float distSq = dx * dx + dy * dy;

	if (distSq <= s.radius * s.radius)
	{
		ICollisionComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
		ICollisionComponent* bNonConst = const_cast<AABBColliderComponent*>(b);
		aNonConst->SetCollisionPosition(s.center);
		// 最近傍点を3Dで返すなら
		Vector3 closestPt = s.center;
		bNonConst->SetCollisionPosition(closestPt);
		return true;
	}
	return false;
}

// Circle vs OBB 2D
bool collisionAlgorithm::CheckCirclevsOBB2D(const SphereColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane)
{
	int axis1, axis2;
	GetPlaneAxes(plane, axis1, axis2);

	const Sphere& s = a->GetSphere();
	const OBB& obb = b->GetOBB();

	// 2D座標
	float sx = GetSizeFromIndex(s.center, axis1);
	float sy = GetSizeFromIndex(s.center, axis2);
	float obb_cx = GetSizeFromIndex(obb.center, axis1);
	float obb_cy = GetSizeFromIndex(obb.center, axis2);

	// OBBの2D軸
	KCE::Vector2 axes[2];
	axes[0] = KCE::Vector2(GetSizeFromIndex(Vector3(obb.rotate.m[axis1][0], obb.rotate.m[axis2][0], 0), 0),
					  GetSizeFromIndex(Vector3(obb.rotate.m[axis1][0], obb.rotate.m[axis2][0], 0), 1));
	axes[1] = KCE::Vector2(GetSizeFromIndex(Vector3(obb.rotate.m[axis1][1], obb.rotate.m[axis2][1], 0), 0),
					  GetSizeFromIndex(Vector3(obb.rotate.m[axis1][1], obb.rotate.m[axis2][1], 0), 1));

	KCE::Vector2 obbCenter(obb_cx, obb_cy);
	KCE::Vector2 circleCenter(sx, sy);
	KCE::Vector2 d = circleCenter - obbCenter;
	KCE::Vector2 closest = obbCenter;

	const float size1 = GetSizeFromIndex(obb.size, axis1);
	const float size2 = GetSizeFromIndex(obb.size, axis2);

	// 各軸ごとに最近傍点を算出
	float dist1 = KCE::Vector2::Dot(d, axes[0]);
	float clamped1 = (std::max)(-size1, (std::min)(dist1, size1));
	closest += axes[0] * clamped1;

	float dist2 = KCE::Vector2::Dot(d, axes[1]);
	float clamped2 = (std::max)(-size2, (std::min)(dist2, size2));
	closest += axes[1] * clamped2;

	KCE::Vector2 diff = circleCenter - closest;
	float distSq = diff.x * diff.x + diff.y * diff.y;

	if (distSq <= s.radius * s.radius)
	{
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

bool collisionAlgorithm::CheckAABBvsAABBSubstep2D(const AABBColliderComponent* a, const AABBColliderComponent* b, CollisionPlane plane)
{
	constexpr float MAX_STEP_DISTANCE = 1.0f;
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const AABB& aBox = a->GetAABB();
	const AABB& bBox = b->GetAABB();

	if (CheckAABBvsAABB2D(a, b, plane)) return true;

	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	float maxDistance = (std::max)(distanceA, distanceB);
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / MAX_STEP_DISTANCE)));

	AABBColliderComponent* aNonConst = const_cast<AABBColliderComponent*>(a);
	AABBColliderComponent* bNonConst = const_cast<AABBColliderComponent*>(b);

	AABBColliderComponent tempA(nullptr);
	AABBColliderComponent tempB(nullptr);

	for (int step = 0; step <= subStepCount; ++step)
	{
		float t = static_cast<float>(step) / subStepCount;
		Vector3 subPosA = MathUtils::Lerp(startA, endA, t);
		Vector3 subPosB = MathUtils::Lerp(startB, endB, t);

		AABB movedAABB_A(subPosA - aBox.GetHalfSize(), subPosA + aBox.GetHalfSize());
		AABB movedAABB_B(subPosB - bBox.GetHalfSize(), subPosB + bBox.GetHalfSize());


		tempA.SetAABB(movedAABB_A);
		tempB.SetAABB(movedAABB_B);

		if (CheckAABBvsAABB2D(&tempA, &tempB, plane))
		{
			aNonConst->SetCollisionPosition(subPosA);
			bNonConst->SetCollisionPosition(subPosB);
			return true;
		}
	}
	return false;
}

bool collisionAlgorithm::CheckOBBvsOBBSubstep2D(const OBBColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane)
{
	constexpr float MAX_STEP_DISTANCE = 1.0f;
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	OBB aObb = a->GetOBB();
	OBB bObb = b->GetOBB();

	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	float maxDistance = (std::max)(distanceA, distanceB);
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / MAX_STEP_DISTANCE)));

	OBBColliderComponent* aNonConst = const_cast<OBBColliderComponent*>(a);
	OBBColliderComponent* bNonConst = const_cast<OBBColliderComponent*>(b);

	OBBColliderComponent tempA(nullptr);
	OBBColliderComponent tempB(nullptr);

	for (int step = 0; step < subStepCount; ++step)
	{
		float t = static_cast<float>(step + 1) / subStepCount;
		Vector3 subPosA = startA + (endA - startA) * t;
		Vector3 subPosB = startB + (endB - startB) * t;

		OBB movedOBB_A = aObb;
		OBB movedOBB_B = bObb;
		movedOBB_A.center = subPosA;
		movedOBB_B.center = subPosB;


		tempA.SetOBB(movedOBB_A);
		tempB.SetOBB(movedOBB_B);

		if (CheckOBBvsOBB2D(&tempA, &tempB, plane))
		{
			aNonConst->SetCollisionPosition(subPosA);
			bNonConst->SetCollisionPosition(subPosB);
			return true;
		}
	}
	return false;
}

bool collisionAlgorithm::CheckAABBvsOBBSubstep2D(const AABBColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane)
{
	constexpr float MAX_STEP_DISTANCE = 1.0f;
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const AABB& aBox = a->GetAABB();
	OBB bObb = b->GetOBB();

	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	float maxDistance = (std::max)(distanceA, distanceB);
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / MAX_STEP_DISTANCE)));

	AABBColliderComponent* aNonConst = const_cast<AABBColliderComponent*>(a);
	OBBColliderComponent* bNonConst = const_cast<OBBColliderComponent*>(b);

	AABBColliderComponent tempA(nullptr);
	OBBColliderComponent tempB(nullptr);

	for (int step = 0; step < subStepCount; ++step)
	{
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

// Circle vs Circle 2D サブステップ
bool collisionAlgorithm::CheckCirclevsCircleSubstep2D(const SphereColliderComponent* a, const SphereColliderComponent* b, CollisionPlane plane)
{
	constexpr float MAX_STEP_DISTANCE = 1.0f;

	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const Sphere& sphereA = a->GetSphere();
	const Sphere& sphereB = b->GetSphere();

	if (CheckCirclevsCircle2D(a, b, plane)) return true;

	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	float maxDistance = (std::max)(distanceA, distanceB);
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / MAX_STEP_DISTANCE)));

	SphereColliderComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
	SphereColliderComponent* bNonConst = const_cast<SphereColliderComponent*>(b);

	int axis1, axis2;
	GetPlaneAxes(plane, axis1, axis2);

	for (int step = 1; step <= subStepCount; ++step)
	{
		float t = static_cast<float>(step) / subStepCount;
		Vector3 subPosA = MathUtils::Lerp(startA, endA, t);
		Vector3 subPosB = MathUtils::Lerp(startB, endB, t);

		float a1 = GetSizeFromIndex(subPosA, axis1);
		float a2 = GetSizeFromIndex(subPosA, axis2);
		float b1 = GetSizeFromIndex(subPosB, axis1);
		float b2 = GetSizeFromIndex(subPosB, axis2);

		float dx = a1 - b1;
		float dy = a2 - b2;
		float distSq = dx * dx + dy * dy;
		float radiusSum = sphereA.radius + sphereB.radius;

		if (distSq <= radiusSum * radiusSum)
		{
			aNonConst->SetCollisionPosition(subPosA);
			bNonConst->SetCollisionPosition(subPosB);
			return true;
		}
	}
	return false;
}

// Circle vs AABB 2D サブステップ
bool collisionAlgorithm::CheckCirclevsAABBSubstep2D(const SphereColliderComponent* a, const AABBColliderComponent* b, CollisionPlane plane)
{
	constexpr float MAX_STEP_DISTANCE = 1.0f;

	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const Sphere& sphereA = a->GetSphere();
	const AABB& boxB = b->GetAABB();

	if (CheckCirclevsAABB2D(a, b, plane)) return true;

	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	float maxDistance = (std::max)(distanceA, distanceB);
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / MAX_STEP_DISTANCE)));

	SphereColliderComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
	AABBColliderComponent* bNonConst = const_cast<AABBColliderComponent*>(b);

	int axis1, axis2;
	GetPlaneAxes(plane, axis1, axis2);

	Vector3 boxHalf = boxB.GetHalfSize();

	for (int step = 1; step <= subStepCount; ++step)
	{
		float t = static_cast<float>(step) / subStepCount;
		Vector3 subPosA = MathUtils::Lerp(startA, endA, t);
		Vector3 subPosB = MathUtils::Lerp(startB, endB, t);

		float cx = GetSizeFromIndex(subPosA, axis1);
		float cy = GetSizeFromIndex(subPosA, axis2);

		float minX = GetSizeFromIndex(subPosB - boxHalf, axis1);
		float minY = GetSizeFromIndex(subPosB - boxHalf, axis2);
		float maxX = GetSizeFromIndex(subPosB + boxHalf, axis1);
		float maxY = GetSizeFromIndex(subPosB + boxHalf, axis2);

		float closestX = (std::max)(minX, (std::min)(cx, maxX));
		float closestY = (std::max)(minY, (std::min)(cy, maxY));

		float dx = cx - closestX;
		float dy = cy - closestY;
		float distSq = dx * dx + dy * dy;

		if (distSq <= sphereA.radius * sphereA.radius)
		{
			aNonConst->SetCollisionPosition(subPosA);
			// 最近傍点を3Dで返すなら
			Vector3 closestPt = subPosA;
			bNonConst->SetCollisionPosition(closestPt);
			return true;
		}
	}
	return false;
}

// Circle vs OBB 2D サブステップ
bool collisionAlgorithm::CheckCirclevsOBBSubstep2D(const SphereColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane)
{
	constexpr float MAX_STEP_DISTANCE = 1.0f;

	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const Sphere& sphereA = a->GetSphere();
	OBB obbB = b->GetOBB();

	if (CheckCirclevsOBB2D(a, b, plane)) return true;

	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	float maxDistance = (std::max)(distanceA, distanceB);
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / MAX_STEP_DISTANCE)));

	SphereColliderComponent* aNonConst = const_cast<SphereColliderComponent*>(a);
	OBBColliderComponent* bNonConst = const_cast<OBBColliderComponent*>(b);

	int axis1, axis2;
	GetPlaneAxes(plane, axis1, axis2);

	for (int step = 1; step <= subStepCount; ++step)
	{
		float t = static_cast<float>(step) / subStepCount;
		Vector3 subPosA = MathUtils::Lerp(startA, endA, t);
		Vector3 subPosB = MathUtils::Lerp(startB, endB, t);

		OBB movedOBB = obbB;
		movedOBB.center = subPosB;

		// 2D座標
		float sx = GetSizeFromIndex(subPosA, axis1);
		float sy = GetSizeFromIndex(subPosA, axis2);
		float obb_cx = GetSizeFromIndex(movedOBB.center, axis1);
		float obb_cy = GetSizeFromIndex(movedOBB.center, axis2);

		KCE::Vector2 axes[2];
		axes[0] = KCE::Vector2(GetSizeFromIndex(Vector3(movedOBB.rotate.m[axis1][0], movedOBB.rotate.m[axis2][0], 0), 0),
						  GetSizeFromIndex(Vector3(movedOBB.rotate.m[axis1][0], movedOBB.rotate.m[axis2][0], 0), 1));
		axes[1] = KCE::Vector2(GetSizeFromIndex(Vector3(movedOBB.rotate.m[axis1][1], movedOBB.rotate.m[axis2][1], 0), 0),
						  GetSizeFromIndex(Vector3(movedOBB.rotate.m[axis1][1], movedOBB.rotate.m[axis2][1], 0), 1));

		KCE::Vector2 obbCenter(obb_cx, obb_cy);
		KCE::Vector2 circleCenter(sx, sy);
		KCE::Vector2 d = circleCenter - obbCenter;
		KCE::Vector2 closest = obbCenter;

		const float size1 = GetSizeFromIndex(movedOBB.size, axis1);
		const float size2 = GetSizeFromIndex(movedOBB.size, axis2);

		float dist1 = KCE::Vector2::Dot(d, axes[0]);
		float clamped1 = (std::max)(-size1, (std::min)(dist1, size1));
		closest += axes[0] * clamped1;

		float dist2 = KCE::Vector2::Dot(d, axes[1]);
		float clamped2 = (std::max)(-size2, (std::min)(dist2, size2));
		closest += axes[1] * clamped2;

		KCE::Vector2 diff = circleCenter - closest;
		float distSq = diff.x * diff.x + diff.y * diff.y;

		if (distSq <= sphereA.radius * sphereA.radius)
		{
			aNonConst->SetCollisionPosition(subPosA);
			// 最近傍点を3Dで返す
			Vector3 closestPt = subPosA;
			bNonConst->SetCollisionPosition(closestPt);
			return true;
		}
	}
	return false;
}
} // namespace KCE
