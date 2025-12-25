#include "AssaultRifleComponent.h"

// app
#include <application/GameObject/base/GameObject.h>
#include "application/GameObject/Combatable/character/player/Player.h"
#include "application/GameObject/Combatable/character/enemy/base/EnemyBase.h"
#include "application/effect/BulletTrailManager.h"
// system
#include "graphics/3d/Object3dCommon.h"
#include "input/Input.h"
// component
#include "BulletComponent.h"
#include "application/GameObject/component/collision/OBBColliderComponent.h"
// math
#include "math/MathUtils.h"
#include "time/TimeManager.h"

// コンストラクタ：武器の初期化
AssaultRifleComponent::AssaultRifleComponent(Object3dCommon* object3dCommon, LightManager* lightManager)
	: fireCooldown_(kDefaultFireCooldown), fireCooldownTimer_(0.0f)
{
	object3dCommon_ = object3dCommon;
	lightManager_ = lightManager;

	// ヒットエフェクトの初期化
	hitEffect_ = std::make_unique<AssaultRifleHitEffect>();
	hitEffect_->Initialize();
}

// デストラクタ：弾のクリーンアップ
AssaultRifleComponent::~AssaultRifleComponent()
{
	// 発射された弾をすべて削除
	for (auto& bullet : bullets_)
	{
		bullet.reset();
	}
	bullets_.clear();
}

// フレームごとの更新処理
void AssaultRifleComponent::Update(GameObject* owner)
{
	float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;

	// クールダウンタイマーを減少
	fireCooldownTimer_ -= deltaTime;

	// リロード処理
	if (isReloading_)
	{
		Reload(deltaTime);
	}

	// プレイヤーの場合の入力処理
	if (auto player = dynamic_cast<Player*>(owner))
	{
		// 選択中の武器のみ入力処理を行う
		if (IsActive())
		{
			// マウス左クリックで発射（クールダウン終了かつ弾がある場合）
			if (Input::GetInstance()->IsMouseButtonPressed(0) && fireCooldownTimer_ <= 0.0f && currentAmmo_ > 0)
			{
				FireBullet(owner);
				fireCooldownTimer_ = fireCooldown_;
				currentAmmo_--;
				// 弾がなくなったらリロード開始
				if (currentAmmo_ <= 0)
				{
					StartReload();
				}
			}

			// Rキーで手動リロード
			if (Input::GetInstance()->TriggerKey(DIK_R) && currentAmmo_ < maxAmmo_)
			{
				StartReload();
			}
		}
	}
	// 敵の場合の処理
	else if (auto enemy = dynamic_cast<EnemyBase*>(owner))
	{
		// 敵のポインタを保持（Fire()メソッドで使用）
		enemy_ = enemy;

		// 弾がなくなったら自動リロード
		if (currentAmmo_ <= 0 && !isReloading_)
		{
			StartReload();
		}
	}

	// すべての弾を更新
	for (const auto& bullet : bullets_)
		if (bullet->IsAlive()) bullet->Update();

	// 死んだ弾を削除（トレイルも解除）
	for (auto it = bullets_.begin(); it != bullets_.end();)
		if (!(*it)->IsAlive())
		{
			// トレイルを解除
			BulletTrailManager::GetInstance().UnregisterBullet((*it)->GetTrailId());
			it = bullets_.erase(it);
		}
		else ++it;
}

// 描画処理
void AssaultRifleComponent::Draw3D(CameraManager* camera)
{
	// 弾のモデル描画は行わない（トレイルエフェクトで表現）
	// トレイルはBulletTrailManagerで一括描画される
	(void)camera;  // 未使用警告回避
}

// 敵クラスから呼び出す発射メソッド
void AssaultRifleComponent::Fire()
{
	// enemyがNullなら処理しない
	if (enemy_ == nullptr) { return; }

	// 自身とプレイヤーの位置を取得
	Vector3 myPos = enemy_->GetPosition();
	Vector3 playerPos = enemy_->GetTarget()->GetPosition();
	float distance = (playerPos - myPos).Length();

	// 発射可能距離内かつ発射条件を満たす場合
	if (distance < kMaxFireDistance && fireCooldownTimer_ <= 0.0f && currentAmmo_ > 0 && !isReloading_)
	{
		FireBullet(enemy_, playerPos);
		fireCooldownTimer_ = fireCooldown_;
		currentAmmo_--;
		// 弾がなくなったらリロード開始
		if (currentAmmo_ <= 0) StartReload();
	}
}

// プレイヤー用の弾発射処理
void AssaultRifleComponent::FireBullet(GameObject* owner)
{
	// 弾の作成
	auto bullet = std::make_unique<Bullet>(GameObjectTag::Weapon::PlayerBullet);

	// カメラを取得
	Camera* camera = object3dCommon_->GetDefaultCamera();
	if (!camera) return;

	// マウスのスクリーン座標を取得
	float mouseX = Input::GetInstance()->GetMouseX();
	float mouseY = Input::GetInstance()->GetMouseY();

	// ビューポート行列を作成
	Matrix4x4 matViewport = MakeViewportMatrix(0, 0, WinApp::kClientWidth, WinApp::kClientHeight, 0, 1);

	// ビュー行列とプロジェクション行列を合成
	Matrix4x4 matVPV = (camera->GetViewMatrix() * camera->GetProjectionMatrix()) * matViewport;

	// 合成行列の逆行列を計算
	Matrix4x4 matInverseVPV = Inverse(matVPV);

	// スクリーン座標を定義（近点と遠点）
	Vector3 posNear = Vector3(mouseX, mouseY, 0.0f);
	Vector3 posFar = Vector3(mouseX, mouseY, 1.0f);

	// スクリーン座標をワールド座標に変換
	posNear = MathUtils::Transform(posNear, matInverseVPV);
	posFar = MathUtils::Transform(posFar, matInverseVPV);

	// プレイヤーの位置を取得
	Vector3 playerPos = owner->GetPosition();

	// レイの方向を計算
	Vector3 rayDir = Vector3::Normalize(posFar - posNear);

	// プレイヤーと同じ高さの平面との交点を計算
	float t = (playerPos.y - posNear.y) / rayDir.y;
	Vector3 targetPos = posNear + rayDir * t;

	// 発射方向を計算
	Vector3 direction = Vector3::Normalize(targetPos - playerPos);

	// 水平方向の角度を計算
	Vector3 horizontalDir = Vector3(direction.x, 0.0f, direction.z);
	horizontalDir = Vector3::Normalize(horizontalDir);
	float rotationY = atan2f(horizontalDir.x, horizontalDir.z);

	// 弾の初期化（モデルは使わない）
	bullet->Initialize(object3dCommon_, lightManager_, playerPos);
	bullet->SetPosition(playerPos);
	bullet->SetRotation({ 0.0f, rotationY, 0.0f });
	bullet->SetScale(Vector3(kBulletScale, kBulletScale, 1.0f));

	// トレイルを登録
	uint32_t trailId = BulletTrailManager::GetInstance().RegisterBullet(bullet->GetTransform());
	bullet->SetTrailId(trailId);

	// BulletComponentを追加
	auto bulletComp = std::make_unique<BulletComponent>();
	bulletComp->Initialize(direction, speed_, lifetime_);
	bullet->AddComponent("Bullet", std::move(bulletComp));

	// 衝突判定コンポーネントを追加
	auto colliderComp = std::make_unique<OBBColliderComponent>(bullet.get());
	colliderComp->SetOnEnter([ptr = bullet.get(), hitEffect = hitEffect_.get()](GameObject* other) {
		// 敵に当たった場合、パーティクルを生成して弾を消す
		if (other->GetTag() == GameObjectTag::Character::PistolEnemy ||
			other->GetTag() == GameObjectTag::Character::AssaultEnemy ||
			other->GetTag() == GameObjectTag::Character::ShotgunEnemy ||
			other->GetTag() == GameObjectTag::Character::KnifeEnemy
			)
		{
			hitEffect->Play(ptr->GetPosition());
			ptr->SetAlive(false);
		}
		// 障害物に当たった場合、弾を消す
		if (other->GetTag() == GameObjectTag::Item::Obstacle)
		{
			ptr->SetAlive(false);
		}
							 });
	bullet->AddComponent("OBBCollider", std::move(colliderComp));

	// 弾を管理リストに追加
	bullets_.push_back(std::move(bullet));
}

// 敵用の弾発射処理（ターゲット指定）
void AssaultRifleComponent::FireBullet(GameObject* owner, const Vector3& targetPosition)
{
	// 弾の作成
	auto bullet = std::make_unique<Bullet>(GameObjectTag::Weapon::EnemyBullet);

	// 発射元の位置を取得
	Vector3 startPos = owner->GetPosition();

	// 発射方向を計算（水平方向のみ）
	Vector3 direction = Vector3::Normalize(targetPosition - startPos);
	direction.y = 0.0f;

	// 水平方向の角度を計算
	float rotationY = atan2f(direction.x, direction.z);

	// 弾の初期化（モデルは使わない）
	bullet->Initialize(object3dCommon_, lightManager_, startPos);
	bullet->SetRotation({ 0.0f, rotationY, 0.0f });
	bullet->SetScale(Vector3(kBulletScale, kBulletScale, 1.0f));

	// トレイルを登録
	uint32_t trailId = BulletTrailManager::GetInstance().RegisterBullet(bullet->GetTransform());
	bullet->SetTrailId(trailId);

	// BulletComponentを追加
	auto bulletComp = std::make_unique<BulletComponent>();
	bulletComp->Initialize(direction, speed_, lifetime_);
	bullet->AddComponent("Bullet", std::move(bulletComp));

	// 衝突判定コンポーネントを追加
	auto colliderComp = std::make_unique<OBBColliderComponent>(bullet.get());

	// 衝突したときの処理を設定
	colliderComp->SetOnEnter([ptr = bullet.get(), hitEffect = hitEffect_.get()](GameObject* other) {
		// プレイヤーに当たった場合、パーティクルを生成して弾を消す
		if (other->GetTag() == GameObjectTag::Character::Player)
		{
			hitEffect->Play(other->GetPosition());
			ptr->SetAlive(false);
		}
		// 障害物に当たった場合、弾を消す
		if (other->GetTag() == GameObjectTag::Item::Obstacle)
		{
			ptr->SetAlive(false);
		}
							 });

	// 当たり判定を登録
	bullet->AddComponent("OBBCollider", std::move(colliderComp));

	// 弾を管理リストに追加
	bullets_.push_back(std::move(bullet));
}

// リロード開始
void AssaultRifleComponent::StartReload()
{
	isReloading_ = true;
	reloadTimer_ = 0.0f;
}

// リロード処理
void AssaultRifleComponent::Reload(float deltaTime)
{
	reloadTimer_ += deltaTime;

	// リロード完了
	if (reloadTimer_ >= reloadTime_)
	{
		currentAmmo_ = maxAmmo_;
		isReloading_ = false;
	}
}
