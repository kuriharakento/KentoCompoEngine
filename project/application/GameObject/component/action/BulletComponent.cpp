#include "BulletComponent.h"  
#include "application/GameObject/base/GameObject.h" // GameObject の完全な型を使用するために追加  
#include "application/GameObject/weapon/Bullet.h"
#include "time/TimeManager.h"

BulletComponent::BulletComponent()
	: speed_(0.0f), lifetime_(0.0f), timeAlive_(0.0f)
{
}

void BulletComponent::Initialize(Vector3 direction, float speed, float lifetime)
{
	direction_ = direction;
	speed_ = speed;
	lifetime_ = lifetime;
	timeAlive_ = 0.0f;
}

void BulletComponent::Update(GameObject* owner)
{
	timeAlive_ += TimeManager::GetInstance().GetDeltaTime();  // 経過時間を更新  

	//ダイナミックキャストで GameObject を取得
	auto bullet = dynamic_cast<Bullet*>(owner);

	// 弾の移動  
	owner->SetPosition(owner->GetPosition() + direction_ * speed_ * TimeManager::GetInstance().GetDeltaTime());

	// ライフタイムを超えたら弾を削除  
	if (timeAlive_ >= lifetime_)
	{
		bullet->SetActive(false);  // 弾を非アクティブにすることで削除処理を行う  
	}
}