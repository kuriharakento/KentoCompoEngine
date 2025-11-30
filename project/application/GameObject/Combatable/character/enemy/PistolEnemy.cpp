#include "PistolEnemy.h"

#include "application/GameObject/component/action/PistolComponent.h"
#include "application/GameObject/component/action/ShotgunComponent.h"
#include "application/GameObject/component/collision/OBBColliderComponent.h"

void PistolEnemy::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, GameObject* target, const Transform& initialTransform)
{
	EnemyBase::Initialize(object3dCommon, lightManager, target, initialTransform);

	//銃射撃のコンポーネントを追加
	AddComponent("PistolComponent", std::make_unique<PistolComponent>(object3dCommon, lightManager));

	// 衝突判定コンポーネントを追加
	auto collider = std::make_unique<OBBColliderComponent>(this);
	collider->SetOnEnter([this](GameObject* other) {
		if (other->GetTag() == GameObjectTag::Weapon::PlayerBullet)
		{
			SetAlive(false); // 弾に当たったら消える
		}
						});
	collider->SetOnStay([this](GameObject* other) {
		// 衝突中の処理をここに記述
						});
	collider->SetOnExit([this](GameObject* other) {
		// 衝突終了時の処理をここに記述
						});
	AddComponent("OBBCollider", std::move(collider));
}