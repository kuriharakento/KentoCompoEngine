#include "KnifeEnemy.h"

#include "application/GameObject/Combatable/weapon/Knife.h"
#include "application/GameObject/component/action/GravityPhysicsComponent.h"
#include "application/GameObject/component/action/KnifeEnemyBehavior.h"
#include "application/GameObject/component/collision/OBBColliderComponent.h"

void KnifeEnemy::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, GameObject* target, const Transform& inintialTransform)
{
	EnemyBase::Initialize(object3dCommon, lightManager, target, inintialTransform);

	// 腕、ナイフのゲームオブジェクトを作成
	// 右腕
	auto rightArm = std::make_unique<GameObject>(GameObjectTag::Character::KnifeEnemyRightArm);
	rightArm->Initialize(object3dCommon, lightManager);
	rightArm->SetModel("cube"); // モデルを設定（適宜変更）
	rightArm->SetPosition(Vector3(1.5f, 0.0f, 0.0f));
	rightArm->SetScale(Vector3(0.5f, 1.0f, 0.5f));
	rightArm->SetRotation(Vector3(0.0f, 0.0f, 0.3f));
	rightArm->AddComponent("OBBColliderComponent", std::make_unique<OBBColliderComponent>(rightArm.get()));

	// 左腕
	auto leftArm = std::make_unique<GameObject>(GameObjectTag::Character::KnifeEnemyLeftArm);
	leftArm->Initialize(object3dCommon, lightManager);
	leftArm->SetModel("cube"); // モデルを設定（適宜変更）
	leftArm->SetPosition(Vector3(-1.5f, 0.0f, 0.0f));
	leftArm->SetScale(Vector3(0.5f, 1.0f, 0.5f));
	leftArm->SetRotation(Vector3(0.0f, 0.0f, -0.3f));
	leftArm->AddComponent("OBBColliderComponent", std::make_unique<OBBColliderComponent>(leftArm.get()));

	// ナイフ
	auto knife = std::make_unique<Knife>(GameObjectTag::Weapon::Knife);
	knife->Initialize(object3dCommon, lightManager);
	knife->SetPosition(Vector3(0.0f, -0.8f, 4.0f)); // ナイフの位置を設定
	knife->SetScale(Vector3(0.3f, 0.3f, 4.0f)); // ナイフのスケールを設定
	knife->AddComponent("OBBColliderComponent", std::make_unique<OBBColliderComponent>(knife.get()));



	// ビヘイビアコンポーネントを追加
	AddComponent("KnifeEnemyBehavior", std::make_unique<KnifeEnemyBehavior>(target, rightArm.get(), leftArm.get(), knife.get()));
	// 重力演算コンポーネントを追加
	AddComponent("GravityPhysicsComponent", std::make_unique<GravityPhysicsComponent>());
	// OBBコライダーコンポーネントを追加
	AddComponent("OBBColliderComponent", std::make_unique<OBBColliderComponent>(this));

	// 右腕にナイフを子オブジェクトとして追加
	rightArm->AddChild(GameObjectTag::Weapon::Knife, std::move(knife));
	// 右腕と左腕を敵キャラクターに追加
	AddChild(GameObjectTag::Character::KnifeEnemyRightArm, std::move(rightArm));
	AddChild(GameObjectTag::Character::KnifeEnemyLeftArm, std::move(leftArm));
}

void KnifeEnemy::Update()
{
	EnemyBase::Update();
}

void KnifeEnemy::Draw(CameraManager* camera)
{
	EnemyBase::Draw(camera);
}

void KnifeEnemy::CollisionSettings(ICollisionComponent* collider)
{
	// スイープ判定を使用
	collider->SetUseSubstep(true);
	// 衝突時の処理を設定
	collider->SetOnEnter([this](GameObject* other) {
		if (other->GetTag() == GameObjectTag::Weapon::PlayerBullet)
		{
			auto combatable = dynamic_cast<CombatableObject*>(other);
			if (combatable)
			{
				// ダメージを受ける
				TakeDamage(combatable->GetAttackPower());
			}
		}
						 });
	collider->SetOnStay([this](GameObject* other) {
		// 衝突中の処理
						});
	collider->SetOnExit([this](GameObject* other) {
		// 衝突が離れた時の処理
						});
}
