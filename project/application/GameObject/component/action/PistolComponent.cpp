#include "PistolComponent.h"

// system
#include "graphics/3d/Object3dCommon.h"
// app
#include <engine/gameobject/base/GameObject.h>
#include "application/gameObject/combatable/character/enemy/base/EnemyBase.h"
#include "application/gameObject/combatable/character/player/Player.h"
// component
#include "engine/gameobject/component/collision/OBBColliderComponent.h"
#include "BulletComponent.h"
#include "time/TimeManager.h"

// コンストラクタ：武器の初期化
PistolComponent::PistolComponent(Object3dCommon* object3dCommon, LightManager* lightManager) : fireCooldown_(kDefaultFireCooldown), fireCooldownTimer_(0.0f)
{
	object3dCommon_ = object3dCommon;
	lightManager_ = lightManager;
}

// デストラクタ：弾のクリーンアップ
PistolComponent::~PistolComponent()
{
	// 発射された弾をすべて削除
	for (auto& bullet : bullets_)
	{
		bullet.reset();
	}
	bullets_.clear();
}

// フレームごとの更新処理
void PistolComponent::Update(GameObject* owner)
{
	// プレイヤーが所有している場合は realDeltaTime を使用してスローモーションを無視する
	bool isPlayerOwner = dynamic_cast<Player*>(owner) != nullptr;
	float deltaTime = isPlayerOwner ? TimeManager::GetInstance().GetGameContext().realDeltaTime : TimeManager::GetInstance().GetGameContext().deltaTime;

	// クールダウンタイマーを減少
	fireCooldownTimer_ -= deltaTime;

	// リロード処理
	if (isReloading_)
	{
		Reload(deltaTime);
	}

	if (!isReloading_)
	{
		// プレイヤーの場合の入力処理
		if (auto player = dynamic_cast<Player*>(owner))
		{
			// マウス左クリックで発射
			if (Input::GetInstance()->IsMouseButtonTriggered(0) && fireCooldownTimer_ <= 0.0f && currentAmmo_ > 0)
			{
				FireBullet(owner);
				fireCooldownTimer_ = fireCooldown_;
				currentAmmo_--;
				// 弾がなくなったらリロード開始
				if (currentAmmo_ <= 0) StartReload();
			}
			// Rキーで手動リロード
			if (Input::GetInstance()->TriggerKey(DIK_R) && currentAmmo_ < maxAmmo_)
			{
				StartReload();
			}
		}
		// 敵の場合の処理
		else if (auto enemy = dynamic_cast<EnemyBase*>(owner))
		{
			Player* player = dynamic_cast<Player*>(enemy->GetTarget());
			if (player)
			{
				// 自身とプレイヤーの位置を取得
				Vector3 myPos = enemy->GetPosition();
				Vector3 playerPos = player->GetPosition();
				float distance = (playerPos - myPos).Length();

				// 発射可能距離内かつ発射条件を満たす場合
				if (distance < kMaxFireDistance && fireCooldownTimer_ <= 0.0f && currentAmmo_ > 0)
				{
					FireBullet(owner, playerPos);
					fireCooldownTimer_ = fireCooldown_;
					currentAmmo_--;
					// 弾がなくなったらリロード開始
					if (currentAmmo_ <= 0) StartReload();
				}
			}
		}
	}

	// すべての弾を更新
	for (const auto& bullet : bullets_)
		if (bullet->IsAlive()) bullet->Update();

	// 死んだ弾を削除
	for (auto it = bullets_.begin(); it != bullets_.end();)
		if (!(*it)->IsAlive()) it = bullets_.erase(it);
		else ++it;
}

// 描画処理
void PistolComponent::Draw3D(CameraManager* camera)
{
	// 生存している弾のみ描画
	for (const auto& bullet : bullets_)
	{
		if (bullet->IsAlive())
		{
			bullet->Draw3D(camera);
		}
	}
}

// プレイヤー用の弾発射処理
void PistolComponent::FireBullet(GameObject* owner)
{
	// 弾の作成
	auto bullet = std::make_unique<Bullet>(gameObjectTag::weapon::PlayerBullet);

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

	// レイの方向を計算
	Vector3 rayDir = Vector3::Normalize(posFar - posNear);

	// プレイヤーの位置を取得
	Vector3 playerPos = owner->GetPosition();

	// 地面（Y=0）との交点を計算
	float t = -posNear.y / rayDir.y;
	Vector3 targetPos = posNear + rayDir * t;

	// 発射方向を計算（水平方向のみ）
	Vector3 direction = Vector3::Normalize(targetPos - playerPos);
	direction.y = 0.0f;

	// 水平方向の角度を計算
	float rotationY = atan2f(direction.x, direction.z);

	// 弾の初期化
	bullet->Initialize(object3dCommon_, lightManager_, playerPos);
	bullet->SetModel("cube.obj");
	bullet->SetRotation({ 0.0f, rotationY, 0.0f });
	bullet->SetScale(Vector3(kBulletScale, kBulletScale, 1.0f));

	// BulletComponentを追加
	auto bulletComp = std::make_unique<BulletComponent>();
	bulletComp->Initialize(direction, kBulletSpeed, kBulletLifetime);
	bulletComp->SetIgnoreTimeScale(true);
	bullet->AddComponent("Bullet", std::move(bulletComp));

	// 衝突判定コンポーネントを追加
	auto colliderComp = std::make_unique<OBBColliderComponent>(bullet.get());
	colliderComp->SetOnEnter([ptr = bullet.get()](GameObject* other) {
		// 敵に当たった場合、弾を消す
		if (other->GetTag() == gameObjectTag::character::PistolEnemy || other->GetTag() == gameObjectTag::character::AssaultEnemy || other->GetTag() == gameObjectTag::character::ShotgunEnemy)
		{
			ptr->SetAlive(false);
		}
							 });

	bullet->AddComponent("OBBCollider", std::move(colliderComp));

	// 弾を管理リストに追加
	bullets_.push_back(std::move(bullet));
}

// 敵用の弾発射処理（ターゲット指定）
void PistolComponent::FireBullet(GameObject* owner, const Vector3& targetPosition)
{
	// 弾の作成
	auto bullet = std::make_unique<Bullet>(gameObjectTag::weapon::EnemyBullet);

	// 発射元の位置を取得
	Vector3 startPos = owner->GetPosition();

	// 発射方向を計算（水平方向のみ）
	Vector3 direction = Vector3::Normalize(targetPosition - startPos);
	direction.y = 0.0f;

	// 水平方向の角度を計算
	float rotationY = atan2f(direction.x, direction.z);

	// 弾の初期化
	bullet->Initialize(object3dCommon_, lightManager_, startPos);
	bullet->SetModel("cube.obj");
	bullet->SetRotation({ 0.0f, rotationY, 0.0f });
	bullet->SetScale(Vector3(kBulletScale, kBulletScale, 1.0f));

	// BulletComponentを追加
	auto bulletComp = std::make_unique<BulletComponent>();
	bulletComp->Initialize(direction, kBulletSpeed, kBulletLifetime);
	bullet->AddComponent("Bullet", std::move(bulletComp));

	// 衝突判定コンポーネントを追加
	auto colliderComp = std::make_unique<OBBColliderComponent>(bullet.get());

	// 衝突したときの処理を設定
	colliderComp->SetOnEnter([ptr = bullet.get()](GameObject* other) {
		// プレイヤーに当たった場合、弾を消す
		if (other->GetTag() == gameObjectTag::character::Player)
		{
			ptr->SetAlive(false);
		}
							 });

	bullet->AddComponent("OBBCollider", std::move(colliderComp));

	// 弾を管理リストに追加
	bullets_.push_back(std::move(bullet));
}

// リロード開始
void PistolComponent::StartReload()
{
	isReloading_ = true;
	reloadTimer_ = 0.0f;
}

// リロード処理
void PistolComponent::Reload(float deltaTime)
{
	reloadTimer_ += deltaTime;

	// リロード完了
	if (reloadTimer_ >= reloadTime_)
	{
		currentAmmo_ = maxAmmo_;
		isReloading_ = false;
	}
}
