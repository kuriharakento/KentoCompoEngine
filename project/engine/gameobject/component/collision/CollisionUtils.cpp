#include "CollisionUtils.h"
#include "engine/gameobject/component/collision/CollisionAlgorithm.h"
#include "OBBColliderComponent.h"
#include "engine/gameobject/base/GameObject.h"

namespace collisionUtils
{
	void ResolvePenetration(GameObject* self, GameObject* other)
	{
		auto selfColl = self->GetComponent<OBBColliderComponent>();
		auto otherColl = other->GetComponent<OBBColliderComponent>();
		if (!selfColl || !otherColl) return;

		Vector3 mtv;
		// 共通のサブステップ対応判定関数を使用。selfColl を動かす方向の MTV を取得。
		if (collisionAlgorithm::CheckOBBvsOBBSubstepMTV(
			otherColl->GetOBB(), otherColl->GetPreviousPosition(),
			selfColl->GetOBB(), selfColl->GetPreviousPosition(), mtv))
		{
			// MTVに沿って自分を押し戻す
			self->SetPosition(self->GetPosition() + mtv);
		}
	}
}
