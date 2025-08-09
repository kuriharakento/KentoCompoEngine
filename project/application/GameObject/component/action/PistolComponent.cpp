#include "PistolComponent.h"

// system
#include "graphics/3d/Object3dCommon.h"
// app
#include <application/GameObject/base/GameObject.h>
#include "application/GameObject/character/enemy/base/EnemyBase.h"
#include "application/GameObject/character/player/Player.h"
// component
#include "application/GameObject/component/collision/OBBColliderComponent.h"
#include "BulletComponent.h"
#include "time/TimeManager.h"

PistolComponent::PistolComponent(Object3dCommon* object3dCommon, LightManager* lightManager) : fireCooldown_(0.5f), fireCooldownTimer_(0.0f)
{
	object3dCommon_ = object3dCommon;  // オブジェクトの共通情報
	lightManager_ = lightManager;  // ライトマネージャー
}

PistolComponent::~PistolComponent()
{
	// デストラクタで発射された弾をすべて削除
	for (auto& bullet : bullets_)
	{
		bullet.reset();  // 弾のポインタをリセット
	}
	bullets_.clear();  // 弾リストをクリア
}

void PistolComponent::Update(GameObject* owner)
{
	float deltaTime = TimeManager::GetInstance().GetDeltaTime();
	fireCooldownTimer_ -= deltaTime;

	// リロード処理
	if (isReloading_)
	{
		Reload(deltaTime);
	}

	if (!isReloading_)
	{
		if (auto player = dynamic_cast<Player*>(owner))
		{
			if (Input::GetInstance()->IsMouseButtonTriggered(0) && fireCooldownTimer_ <= 0.0f && currentAmmo_ > 0)
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
			Player* player = dynamic_cast<Player*>(enemy->GetTarget());
			if (player)
			{
				Vector3 myPos = enemy->GetPosition();
				Vector3 playerPos = player->GetPosition();
				float distance = (playerPos - myPos).Length();
				if (distance < 30.0f && fireCooldownTimer_ <= 0.0f && currentAmmo_ > 0)
				{
					FireBullet(owner, playerPos);
					fireCooldownTimer_ = fireCooldown_;
					currentAmmo_--;
					if (currentAmmo_ <= 0) StartReload();
				}
			}
		}
	}

	// 弾の更新・削除は常に行う
	for (const auto& bullet : bullets_)
		if (bullet->IsAlive()) bullet->Update(deltaTime);

	for (auto it = bullets_.begin(); it != bullets_.end();)
		if (!(*it)->IsAlive()) it = bullets_.erase(it);
		else ++it;
}

void PistolComponent::Draw(CameraManager* camera)
{
	// 弾の描画
	for (const auto& bullet : bullets_)
	{
		if (bullet->IsAlive())
		{
			bullet->Draw(camera);  // 生存している弾のみ描画
		}
	}
}

void PistolComponent::FireBullet(GameObject* owner)
{
	// 弾の作成
	auto bullet = std::make_unique<Bullet>("Bullet");

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

	// レイの方向を計算
	Vector3 rayDir = Vector3::Normalize(posFar - posNear);

	// プレイヤーの位置を取得
	Vector3 playerPos = owner->GetPosition();

	// 地面（Y=0）との交点を計算
	float t = -posNear.y / rayDir.y; // Y=0 の平面との交点
	Vector3 targetPos = posNear + rayDir * t;

	// 発射方向を計算
	Vector3 direction = Vector3::Normalize(targetPos - playerPos);
	direction.y = 0.0f; // Y成分を0にすることで水平方向のベクトルにする

	// 水平方向の角度を計算
	float rotationY = atan2f(direction.x, direction.z);

	// 弾の初期化
	bullet->Initialize(object3dCommon_, lightManager_, playerPos);
	bullet->SetModel("cube.obj");
	bullet->SetRotation({ 0.0f, rotationY, 0.0f });
	bullet->SetScale(Vector3(0.3f, 0.3f, 1.0f));

	// BulletComponentを追加
	auto bulletComp = std::make_unique<BulletComponent>();
	bulletComp->Initialize(direction, 30.0f, 2.0f); // 速度: 10.0f, 寿命: 2.0f
	bullet->AddComponent("Bullet", std::move(bulletComp));

	// 衝突判定コンポーネントを追加
	auto colliderComp = std::make_unique<OBBColliderComponent>(bullet.get());
	colliderComp->SetOnEnter([ptr = bullet.get()](GameObject* other) {
		// 敵に当たった場合、弾を消す
		if (other->GetTag() == "PistolEnemy" || other->GetTag() == "AssaultEnemy" || other->GetTag() == "ShotgunEnemy")
		{
			ptr->SetActive(false);
		}
							 });

	bullet->AddComponent("OBBCollider", std::move(colliderComp));

	// 弾を管理リストに追加
	bullets_.push_back(std::move(bullet));
}

void PistolComponent::FireBullet(GameObject* owner, const Vector3& targetPosition)
{
	// 弾の作成
	auto bullet = std::make_unique<Bullet>("Bullet");

	// 発射元の位置
	Vector3 startPos = owner->GetPosition();

	// 発射方向を計算
	Vector3 direction = Vector3::Normalize(targetPosition - startPos);
	direction.y = 0.0f; // 水平方向のみ撃ちたい場合はY成分を0に

	// 水平方向の角度を計算
	float rotationY = atan2f(direction.x, direction.z);

	// 弾の初期化
	bullet->Initialize(object3dCommon_, lightManager_, startPos);
	bullet->SetModel("cube.obj");
	bullet->SetRotation({ 0.0f, rotationY, 0.0f });
	bullet->SetScale(Vector3(0.3f, 0.3f, 1.0f));

	// BulletComponentを追加
	auto bulletComp = std::make_unique<BulletComponent>();
	bulletComp->Initialize(direction, 30.0f, 2.0f); // 速度: 30.0f, 寿命: 2.0f
	bullet->AddComponent("Bullet", std::move(bulletComp));

	// 衝突判定コンポーネントを追加
	auto colliderComp = std::make_unique<OBBColliderComponent>(bullet.get());

	// 衝突したときの処理を設定
	colliderComp->SetOnEnter([ptr = bullet.get()](GameObject* other) {
		// 敵に当たった場合、弾を消す
		if (other->GetTag() == "Player")
		{
			ptr->SetActive(false);
		}
							 });

	bullet->AddComponent("OBBCollider", std::move(colliderComp));

	// 弾を管理リストに追加
	bullets_.push_back(std::move(bullet));
}

void PistolComponent::StartReload()
{
	isReloading_ = true;
	reloadTimer_ = 0.0f;
}

void PistolComponent::Reload(float deltaTime)
{
	reloadTimer_ += deltaTime;
	if (reloadTimer_ >= reloadTime_)
	{
		currentAmmo_ = maxAmmo_;
		isReloading_ = false;
	}
}
