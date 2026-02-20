#include "CollisionUtils.h"

#include <vector>

#include "OBBColliderComponent.h"
#include "engine/gameobject/base/GameObject.h"

namespace collisionUtils
{
	bool collisionUtils::CheckOBBvsOBBMTV(const OBB& obbA, const OBB& obbB, Vector3& mtv)
	{
		// 各OBBのワールド軸ベクトルを取得
		Matrix4x4 rotA = obbA.rotate;
		Matrix4x4 rotB = obbB.rotate;
		Vector3 axesA[3] = {
			Vector3::Normalize({rotA.m[0][0], rotA.m[0][1], rotA.m[0][2]}),
			Vector3::Normalize({rotA.m[1][0], rotA.m[1][1], rotA.m[1][2]}),
			Vector3::Normalize({rotA.m[2][0], rotA.m[2][1], rotA.m[2][2]})
		};
		Vector3 axesB[3] = {
			Vector3::Normalize({rotB.m[0][0], rotB.m[0][1], rotB.m[0][2]}),
			Vector3::Normalize({rotB.m[1][0], rotB.m[1][1], rotB.m[1][2]}),
			Vector3::Normalize({rotB.m[2][0], rotB.m[2][1], rotB.m[2][2]})
		};

		// 15の分離軸（Aの3軸 + Bの3軸 + 外積9軸）
		std::vector<Vector3> testAxes;
		testAxes.reserve(15);
		for (int i = 0; i < 3; ++i) testAxes.push_back(axesA[i]);
		for (int i = 0; i < 3; ++i) testAxes.push_back(axesB[i]);
		for (int i = 0; i < 3; ++i)
			for (int j = 0; j < 3; ++j)
				testAxes.push_back(Vector3::Normalize(Vector3::Cross(axesA[i], axesB[j])));

		Vector3 toCenter = obbB.center - obbA.center;
		CollisionInfo info;

		// 各分離軸でのめり込み深度を計算
		for (auto& axis : testAxes)
		{
			if (axis.LengthSquared() < 1e-6f) continue;

			float projA = std::abs(Vector3::Dot(axesA[0] * obbA.size.x, axis))
				+ std::abs(Vector3::Dot(axesA[1] * obbA.size.y, axis))
				+ std::abs(Vector3::Dot(axesA[2] * obbA.size.z, axis));
			float projB = std::abs(Vector3::Dot(axesB[0] * obbB.size.x, axis))
				+ std::abs(Vector3::Dot(axesB[1] * obbB.size.y, axis))
				+ std::abs(Vector3::Dot(axesB[2] * obbB.size.z, axis));
			float dist = std::abs(Vector3::Dot(toCenter, axis));
			float overlap = (projA + projB) - dist;

			// 分離軸が見つかった場合は衝突していない
			if (overlap < 0)
			{
				return false;
			}
			
			// 最小のめり込み深度を記録（MTV計算用）
			if (overlap < info.mtvDepth)
			{
				info.isColliding = true;
				info.mtvDepth = overlap;
				info.mtvAxis = axis;
			}
		}

		// 衝突時にMTV（最小変位ベクトル）を算出
		if (info.isColliding)
		{
			// MTVの方向を調整（押し出す方向に統一）
			if (Vector3::Dot(info.mtvAxis, toCenter) < 0.0f)
				info.mtvAxis = info.mtvAxis * -1.0f;
			mtv = info.mtvAxis * info.mtvDepth;
		}
		return info.isColliding;
	}

	void ResolvePenetration(GameObject* self, GameObject* other)
	{
		auto selfColl = self->GetComponent<OBBColliderComponent>();
		auto otherColl = other->GetComponent<OBBColliderComponent>();
		if (!selfColl || !otherColl) return;

		const OBB& obbA = selfColl->GetOBB();
		const OBB& obbB = otherColl->GetOBB();

		Vector3 mtv;
		if (CheckOBBvsOBBMTV(obbA, obbB, mtv))
		{
			// MTVに沿って自分を押し戻す
			self->SetPosition(self->GetPosition() + mtv);
		}
	}
}
