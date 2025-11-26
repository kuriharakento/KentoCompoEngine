#include "CollisionUtils.h"

#include <vector>

#include "OBBColliderComponent.h"
#include "application/GameObject/base/GameObject.h"

namespace
{
	// OBBの軸数
	constexpr int kObbAxisCount = 3;
	
	// OBB vs OBBの分離軸数（各OBBの3軸 + 外積9軸 = 15）
	constexpr int kObbSeparatingAxesCount = 15;
	
	// 軸の長さの最小値（ゼロベクトル判定用許容誤差）
	constexpr float kMinimumAxisLengthSquared = 1e-6f;
}

namespace CollisionUtils
{
	bool CollisionUtils::CheckOBBvsOBBMTV(const OBB& obbA, const OBB& obbB, Vector3& mtv)
	{
		// 各OBBのワールド軸ベクトルを取得（回転行列の各行が軸方向）
		Matrix4x4 rotA = obbA.rotate;
		Matrix4x4 rotB = obbB.rotate;
		Vector3 axesA[kObbAxisCount] = {
			Vector3::Normalize({rotA.m[0][0], rotA.m[0][1], rotA.m[0][2]}),
			Vector3::Normalize({rotA.m[1][0], rotA.m[1][1], rotA.m[1][2]}),
			Vector3::Normalize({rotA.m[2][0], rotA.m[2][1], rotA.m[2][2]})
		};
		Vector3 axesB[kObbAxisCount] = {
			Vector3::Normalize({rotB.m[0][0], rotB.m[0][1], rotB.m[0][2]}),
			Vector3::Normalize({rotB.m[1][0], rotB.m[1][1], rotB.m[1][2]}),
			Vector3::Normalize({rotB.m[2][0], rotB.m[2][1], rotB.m[2][2]})
		};

		// 15の分離軸を構築（Aの3軸 + Bの3軸 + 外積9軸）
		std::vector<Vector3> testAxes;
		testAxes.reserve(kObbSeparatingAxesCount);
		
		// OBB Aの3軸を追加
		for (int i = 0; i < kObbAxisCount; ++i) testAxes.push_back(axesA[i]);
		// OBB Bの3軸を追加
		for (int i = 0; i < kObbAxisCount; ++i) testAxes.push_back(axesB[i]);
		// 外積軸を追加（各軸の組み合わせ）
		for (int i = 0; i < kObbAxisCount; ++i)
			for (int j = 0; j < kObbAxisCount; ++j)
				testAxes.push_back(Vector3::Normalize(Vector3::Cross(axesA[i], axesB[j])));

		// 2つのOBBの中心間ベクトル
		Vector3 toCenter = obbB.center - obbA.center;
		CollisionInfo info;

		// 各分離軸でのめり込み深度を計算
		for (auto& axis : testAxes)
		{
			// ゼロベクトル（平行な軸の外積結果）はスキップ
			if (axis.LengthSquared() < kMinimumAxisLengthSquared) continue;

			// OBB Aの軸への投影サイズを計算
			float projA = std::abs(Vector3::Dot(axesA[0] * obbA.size.x, axis))
				+ std::abs(Vector3::Dot(axesA[1] * obbA.size.y, axis))
				+ std::abs(Vector3::Dot(axesA[2] * obbA.size.z, axis));
			// OBB Bの軸への投影サイズを計算
			float projB = std::abs(Vector3::Dot(axesB[0] * obbB.size.x, axis))
				+ std::abs(Vector3::Dot(axesB[1] * obbB.size.y, axis))
				+ std::abs(Vector3::Dot(axesB[2] * obbB.size.z, axis));
			// 中心間の軸方向距離
			float dist = std::abs(Vector3::Dot(toCenter, axis));
			// めり込み深度を計算（負の場合は分離している）
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
			// MTVの大きさと方向を組み合わせて最終的なベクトルを計算
			mtv = info.mtvAxis * info.mtvDepth;
		}
		return info.isColliding;
	}

	void ResolvePenetration(GameObject* self, GameObject* other)
	{
		// 両方のオブジェクトからOBBコンポーネントを取得
		auto selfColl = self->GetComponent<OBBColliderComponent>();
		auto otherColl = other->GetComponent<OBBColliderComponent>();
		// どちらかがnullならめり込み解決は行わない
		if (!selfColl || !otherColl) return;

		const OBB& obbA = selfColl->GetOBB();
		const OBB& obbB = otherColl->GetOBB();

		// MTV（最小変位ベクトル）を計算
		Vector3 mtv;
		if (CheckOBBvsOBBMTV(obbA, obbB, mtv))
		{
			// MTVに沿って自分を押し戻してめり込みを解消
			self->SetPosition(self->GetPosition() + mtv);
		}
	}
}
