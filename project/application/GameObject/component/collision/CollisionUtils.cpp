#include "CollisionUtils.h"

#include <vector>

#include "OBBColliderComponent.h"
#include "application/GameObject/base/GameObject.h"

namespace CollisionUtils
{
	// ============================================================================
	// 定数定義
	// ============================================================================
	
	// 軸の長さの判定に使用する許容誤差（ゼロベクトル判定）
	constexpr float kEpsilonSquared = 1e-6f;
	
	// OBB判定で使用する軸の数
	constexpr int kObbAxisCount = 3;
	
	// 分離軸の最大数（3 + 3 + 9 = 15）
	constexpr int kMaxSeparatingAxes = 15;

	/**
	 * @brief OBB同士のMTV付き衝突判定
	 * 
	 * 分離軸定理（SAT）を使用してOBB同士の衝突を判定し、
	 * 衝突時には最小変位ベクトル（MTV）を計算します。
	 */
	bool CollisionUtils::CheckOBBvsOBBMTV(const OBB& obbA, const OBB& obbB, Vector3& mtv)
	{
		// 各OBBのワールド軸ベクトルを取得（回転行列の各行を正規化）
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

		// 分離軸を構築（Aの3軸 + Bの3軸 + 外積9軸）
		std::vector<Vector3> testAxes;
		testAxes.reserve(kMaxSeparatingAxes);
		
		// OBB Aの軸を追加
		for (int i = 0; i < kObbAxisCount; ++i) testAxes.push_back(axesA[i]);
		// OBB Bの軸を追加
		for (int i = 0; i < kObbAxisCount; ++i) testAxes.push_back(axesB[i]);
		// 外積で生成される9軸を追加
		for (int i = 0; i < kObbAxisCount; ++i)
			for (int j = 0; j < kObbAxisCount; ++j)
				testAxes.push_back(Vector3::Normalize(Vector3::Cross(axesA[i], axesB[j])));

		// 2つのOBB中心間のベクトル
		Vector3 toCenter = obbB.center - obbA.center;
		CollisionInfo info;

		// 各分離軸でのめり込み深度を計算
		for (auto& axis : testAxes)
		{
			// ゼロベクトル（平行な軸の外積）はスキップ
			if (axis.LengthSquared() < kEpsilonSquared) continue;

			// 各OBBの軸への投影サイズを計算
			float projA = std::abs(Vector3::Dot(axesA[0] * obbA.size.x, axis))
				+ std::abs(Vector3::Dot(axesA[1] * obbA.size.y, axis))
				+ std::abs(Vector3::Dot(axesA[2] * obbA.size.z, axis));
			float projB = std::abs(Vector3::Dot(axesB[0] * obbB.size.x, axis))
				+ std::abs(Vector3::Dot(axesB[1] * obbB.size.y, axis))
				+ std::abs(Vector3::Dot(axesB[2] * obbB.size.z, axis));
			
			// 中心間距離の軸への投影
			float dist = std::abs(Vector3::Dot(toCenter, axis));
			
			// めり込み深度を計算（投影サイズの合計 - 中心間距離）
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
			
			// MTVを計算（方向 × 深度）
			mtv = info.mtvAxis * info.mtvDepth;
		}
		return info.isColliding;
	}

	/**
	 * @brief めり込みを解決する
	 * 
	 * 2つのOBB間のめり込みをMTVを使用して解決します。
	 */
	void ResolvePenetration(GameObject* self, GameObject* other)
	{
		// 両方のGameObjectからOBBColliderComponentを取得
		auto selfColl = self->GetComponent<OBBColliderComponent>();
		auto otherColl = other->GetComponent<OBBColliderComponent>();
		if (!selfColl || !otherColl) return;

		const OBB& obbA = selfColl->GetOBB();
		const OBB& obbB = otherColl->GetOBB();

		// MTVを計算
		Vector3 mtv;
		if (CheckOBBvsOBBMTV(obbA, obbB, mtv))
		{
			// MTVに沿って自分を押し戻す（めり込み解消）
			self->SetPosition(self->GetPosition() + mtv);
		}
	}
}
