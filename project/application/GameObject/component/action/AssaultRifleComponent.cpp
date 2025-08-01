#include "AssaultRifleComponent.h"

// app
#include <application/GameObject/base/GameObject.h>
#include "application/GameObject/character/enemy/base/EnemyBase.h"
#include "application/GameObject/character/player/Player.h"
// system
#include "graphics/3d/Object3dCommon.h"
#include "input/Input.h"
// component
#include "BulletComponent.h"
#include "application/GameObject/component/collision/OBBColliderComponent.h"
// math
#include "math/MathUtils.h"

AssaultRifleComponent::AssaultRifleComponent(Object3dCommon* object3dCommon, LightManager* lightManager)
    : fireCooldown_(0.1f), fireCooldownTimer_(0.0f)
{
    object3dCommon_ = object3dCommon;
    lightManager_ = lightManager;
	hitEffect_ = std::make_unique<AssaultRifleHitEffect>();
	hitEffect_->Initialize();
}

AssaultRifleComponent::~AssaultRifleComponent()
{
    for (auto& bullet : bullets_) bullet.reset();
    bullets_.clear();
}

void AssaultRifleComponent::Update(GameObject* owner)
{
	float deltaTime = 1.0f / 60.0f;
	fireCooldownTimer_ -= deltaTime;

	// リロード処理
	if (isReloading_)
	{
		Reload(deltaTime);
	}

    if (auto player = dynamic_cast<Player*>(owner))
    {
		if (Input::GetInstance()->IsMouseButtonPressed(0) && fireCooldownTimer_ <= 0.0f && currentAmmo_ > 0)
		{
			FireBullet(owner);
			fireCooldownTimer_ = fireCooldown_;
			currentAmmo_--;
			if (currentAmmo_ <= 0) StartReload();
		}

		// 手動リロード
		if (Input::GetInstance()->TriggerKey(DIK_R) && currentAmmo_ < maxAmmo_)
		{
			StartReload();
		}

    }
	else if (auto enemy = dynamic_cast<EnemyBase*>(owner))
	{
		// 敵が任意のタイミングで発射するために敵のポインタを保持
		enemy_ = enemy;

		if (currentAmmo_ <= 0 && !isReloading_)
		{
			StartReload();
		}
	}

    for (const auto& bullet : bullets_)
        if (bullet->IsAlive()) bullet->Update(1.0f / 60.0f);

    for (auto it = bullets_.begin(); it != bullets_.end();)
		if (!(*it)->IsAlive())
		{
			it = bullets_.erase(it);
		}
        else ++it;
}

void AssaultRifleComponent::Draw(CameraManager* camera)
{
    for (const auto& bullet : bullets_)
        if (bullet->IsAlive()) bullet->Draw(camera);
}

void AssaultRifleComponent::Fire()
{
	Vector3 myPos = enemy_->GetPosition();
	Vector3 playerPos = enemy_->GetTarget()->GetPosition();
	float distance = (playerPos - myPos).Length();
	if (distance < 40.0f && fireCooldownTimer_ <= 0.0f && currentAmmo_ > 0 && !isReloading_)
	{
		FireBullet(enemy_, playerPos);
		fireCooldownTimer_ = fireCooldown_;
		currentAmmo_--;
		if (currentAmmo_ <= 0) StartReload();
	}
}


void AssaultRifleComponent::FireBullet(GameObject* owner)
{
	// 弾の作成
	auto bullet = std::make_unique<Bullet>("PlayerBullet");
	// カメラ取得
	Camera* camera = object3dCommon_->GetDefaultCamera();
	if (!camera) return;
	// マウスのスクリーン座標を取得
	float mouseX = Input::GetInstance()->GetMouseX();
	float mouseY = Input::GetInstance()->GetMouseY();
	// ビューポート行列を作成
	Matrix4x4 matViewport = MakeViewportMatrix(0, 0, WinApp::kClientWidth, WinApp::kClientHeight, 0, 1);
	// ビュー行列とプロジェクション行列を取得し、ビューポート行列と合成
	Matrix4x4 matVPV = (camera->GetViewMatrix() * camera->GetProjectionMatrix()) * matViewport;
	// 合成行列の逆行列を計算
	Matrix4x4 matInverseVPV = Inverse(matVPV);
	// スクリーン座標を定義（近点と遠点）
	Vector3 posNear = Vector3(mouseX, mouseY, 0.0f); // 近クリップ面
	Vector3 posFar = Vector3(mouseX, mouseY, 1.0f);  // 遠クリップ面
	// スクリーン座標をワールド座標に変換
	posNear = MathUtils::Transform(posNear, matInverseVPV);
	posFar = MathUtils::Transform(posFar, matInverseVPV);
	// プレイヤーの位置を取得
	Vector3 playerPos = owner->GetPosition();

	// プレイヤーの高さを考慮した交点計算
	Vector3 rayDir = Vector3::Normalize(posFar - posNear);

	// プレイヤーと同じ高さの平面との交点を計算
	float t = (playerPos.y - posNear.y) / rayDir.y;
	Vector3 targetPos = posNear + rayDir * t;

	// 発射方向を計算（Y成分も含める）
	Vector3 direction = Vector3::Normalize(targetPos - playerPos);

	// 水平方向の角度を計算（Y成分を除外した方向で計算）
	Vector3 horizontalDir = Vector3(direction.x, 0.0f, direction.z);
	horizontalDir = Vector3::Normalize(horizontalDir);
	float rotationY = atan2f(horizontalDir.x, horizontalDir.z);

	// 弾の初期化
	bullet->Initialize(object3dCommon_, lightManager_, playerPos);
	bullet->SetModel("bullet");
	bullet->SetRotation({ 0.0f, rotationY, 0.0f });
	bullet->SetScale(Vector3(0.3f, 0.3f, 1.0f));
	// BulletComponentを追加
	auto bulletComp = std::make_unique<BulletComponent>();
	bulletComp->Initialize(direction, speed_, lifetime_);
	bullet->AddComponent("Bullet", std::move(bulletComp));
	// 衝突判定コンポーネントを追加
	auto colliderComp = std::make_unique<OBBColliderComponent>(bullet.get());
	colliderComp->SetOnEnter([ptr = bullet.get(), hitEffect = hitEffect_.get()](GameObject* other) {
		// 敵に当たった場合、パーティクルを生成して弾を消す
		if (other->GetTag() == "PistolEnemy" || other->GetTag() == "AssaultEnemy" || other->GetTag() == "ShotgunEnemy")
		{
			hitEffect->Play(ptr->GetPosition());
			ptr->SetActive(false);
		}
		// 障害物に当たった場合、弾を消す
		if (other->GetTag() == "Obstacle")
		{
			ptr->SetActive(false);
		}
							 });
	bullet->AddComponent("OBBCollider", std::move(colliderComp));
	// 弾を管理リストに追加
	bullets_.push_back(std::move(bullet));
}

void AssaultRifleComponent::FireBullet(GameObject* owner, const Vector3& targetPosition)
{
	// 弾の作成
	auto bullet = std::make_unique<Bullet>("EnemyBullet");

	// 発射元の位置
	Vector3 startPos = owner->GetPosition();

	// 発射方向を計算
	Vector3 direction = Vector3::Normalize(targetPosition - startPos);
	direction.y = 0.0f; // 水平方向のみ撃ちたい場合はY成分を0に

	// 水平方向の角度を計算
	float rotationY = atan2f(direction.x, direction.z);

	// 弾の初期化
	bullet->Initialize(object3dCommon_, lightManager_, startPos);
	bullet->SetModel("bullet");
	bullet->SetRotation({ 0.0f, rotationY, 0.0f });
	bullet->SetScale(Vector3(0.3f, 0.3f, 1.0f));

	// BulletComponentを追加
	auto bulletComp = std::make_unique<BulletComponent>();
	bulletComp->Initialize(direction, speed_, lifetime_);
	bullet->AddComponent("Bullet", std::move(bulletComp));

	// 衝突判定コンポーネントを追加
	auto colliderComp = std::make_unique<OBBColliderComponent>(bullet.get());

	// 衝突したときの処理を設定
	colliderComp->SetOnEnter([ptr = bullet.get() , hitEffect = hitEffect_.get()](GameObject* other) {
		// 敵に当たった場合、パーティクルを生成して弾を消す
		if (other->GetTag() == "Player")
		{
			hitEffect->Play(other->GetPosition());
			ptr->SetActive(false);
		}
		// 障害物に当たった場合、弾を消す
		if (other->GetTag() == "Obstacle")
		{
			ptr->SetActive(false);
		}
							 });

	bullet->AddComponent("OBBCollider", std::move(colliderComp));

	// 弾を管理リストに追加
	bullets_.push_back(std::move(bullet));
}

void AssaultRifleComponent::StartReload()
{
	isReloading_ = true;
	reloadTimer_ = 0.0f;
}

void AssaultRifleComponent::Reload(float deltaTime)
{
	reloadTimer_ += deltaTime;
	if (reloadTimer_ >= reloadTime_)
	{
		currentAmmo_ = maxAmmo_;
		isReloading_ = false;
	}
}
