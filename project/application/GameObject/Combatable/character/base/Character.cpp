#include "Character.h"

void Character::Update()
{
	GameObject::Update();

	// 無敵時間の更新処理
	if (isInvincible_)
	{
		invincibleTimer_ -= 1.0f / 60.0f;
		if (invincibleTimer_ <= 0.0f)
		{
			invincibleTimer_ = 0.0f;
			isInvincible_ = false;
		}
	}
}

void Character::Draw(CameraManager* camera)
{
	GameObject::Draw(camera);
}

void Character::AddComponent(const std::string& name, std::unique_ptr<IGameObjectComponent> comp)
{
	if (auto collider = dynamic_cast<ICollisionComponent*>(comp.get()))
	{
		// 衝突判定コンポーネントの場合、サブクラス固有の衝突設定を適用
		CollisionSettings(collider);
	}
	GameObject::AddComponent(name, std::move(comp));
}

void Character::SetInvincible(float duration)
{
	isInvincible_ = true;
	invincibleTimer_ = duration;
}
