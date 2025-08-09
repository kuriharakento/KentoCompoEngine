#include "ShotgunComponent.h"

// system
#include "graphics/3d/Object3dCommon.h"
#include "input/Input.h"
// app
#include <application/GameObject/base/GameObject.h>
#include "application/GameObject/character/enemy/base/EnemyBase.h"
#include "application/GameObject/character/player/Player.h"
// component
#include "BulletComponent.h"
#include "application/GameObject/component/collision/OBBColliderComponent.h"
// math
#include "math/MathUtils.h"
#include <random>

#include "time/TimeManager.h"

ShotgunComponent::ShotgunComponent(Object3dCommon* object3dCommon, LightManager* lightManager)
    : fireCooldown_(0.8f), fireCooldownTimer_(0.0f)
{
    object3dCommon_ = object3dCommon;
    lightManager_ = lightManager;
}

ShotgunComponent::~ShotgunComponent()
{
    for (auto& bullet : bullets_) bullet.reset();
    bullets_.clear();
}

void ShotgunComponent::Update(GameObject* owner)
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
		//-- プレイヤー用処理 ---
        if (auto player = dynamic_cast<Player*>(owner))
        {
			// プレイヤーの入力で発射
            if (Input::GetInstance()->IsMouseButtonTriggered(0) && fireCooldownTimer_ <= 0.0f && currentAmmo_ > 0)
            {
                FireBullets(owner);
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
		// -- 敵用処理 ---
        else if (auto enemy = dynamic_cast<EnemyBase*>(owner))
        {
			// 敵がプレイヤーに向かって弾を発射
            Player* player = dynamic_cast<Player*>(enemy->GetTarget());
            if (player)
            {
                Vector3 myPos = enemy->GetPosition();
                Vector3 playerPos = player->GetPosition();
                float distance = (playerPos - myPos).Length();
                if (distance < 25.0f && fireCooldownTimer_ <= 0.0f && currentAmmo_ > 0)
                {
                    FireBullets(owner, playerPos);
                    fireCooldownTimer_ = fireCooldown_;
                    currentAmmo_--;
                    if (currentAmmo_ <= 0) StartReload();
                }
            }
        }
    }

    // 弾の更新
    for (const auto& bullet : bullets_)
        if (bullet->IsAlive()) bullet->Update(deltaTime);
	// 生存フラグが立っていない弾を削除
    for (auto it = bullets_.begin(); it != bullets_.end();)
        if (!(*it)->IsAlive()) it = bullets_.erase(it);
        else ++it;
}

void ShotgunComponent::Draw(CameraManager* camera)
{
    for (const auto& bullet : bullets_)
        if (bullet->IsAlive()) bullet->Draw(camera);
}

void ShotgunComponent::FireBullets(GameObject* owner)
{
    // PistolComponent::FireBulletのロジックをベースに、pelletCount_分だけ異なる方向に弾を発射
    Camera* camera = object3dCommon_->GetDefaultCamera();
    if (!camera) return;

    float mouseX = Input::GetInstance()->GetMouseX();
    float mouseY = Input::GetInstance()->GetMouseY();
    Matrix4x4 matViewport = MakeViewportMatrix(0, 0, WinApp::kClientWidth, WinApp::kClientHeight, 0, 1);
    Matrix4x4 matVPV = (camera->GetViewMatrix() * camera->GetProjectionMatrix()) * matViewport;
    Matrix4x4 matInverseVPV = Inverse(matVPV);

    Vector3 posNear = Vector3(mouseX, mouseY, 0.0f);
    Vector3 posFar = Vector3(mouseX, mouseY, 1.0f);
    posNear = MathUtils::Transform(posNear, matInverseVPV);
    posFar = MathUtils::Transform(posFar, matInverseVPV);

    Vector3 rayDir = Vector3::Normalize(posFar - posNear);
    Vector3 playerPos = owner->GetPosition();
    float t = -posNear.y / rayDir.y;
    Vector3 targetPos = posNear + rayDir * t;
    Vector3 baseDir = Vector3::Normalize(targetPos - playerPos);
    baseDir.y = 0.0f;

    float baseAngle = atan2f(baseDir.x, baseDir.z);

    // 乱数生成器の用意
    static std::random_device rd;
    static std::mt19937 gen(rd());
    // ばらけ角度の範囲（例: ±spreadAngle_度）
    std::uniform_real_distribution<float> angleDist(-spreadAngle_, spreadAngle_);
    // Y方向の微小ばらけ（上下にも少し散らす場合）
    std::uniform_real_distribution<float> yDist(-0.05f, 0.05f);

    for (int i = 0; i < pelletCount_; ++i)
    {
        // ランダムな角度オフセット（度→ラジアン変換）
        float angleOffset = angleDist(gen) * 3.14159265f / 180.0f;
        float angle = baseAngle + angleOffset;
        float yOffset = yDist(gen);

        Vector3 dir = { sinf(angle), yOffset, cosf(angle) };
        dir = Vector3::Normalize(dir);

        auto bullet = std::make_unique<Bullet>("Bullet");
        bullet->Initialize(object3dCommon_, lightManager_, playerPos);
        bullet->SetModel("cube.obj");
        bullet->SetRotation({ 0.0f, angle, 0.0f });
        bullet->SetScale(Vector3(0.3f, 0.3f, 1.0f));

        auto bulletComp = std::make_unique<BulletComponent>();
        bulletComp->Initialize(dir, 20.0f, 1.0f); // 速度・寿命はショットガン用に調整
        bullet->AddComponent("Bullet", std::move(bulletComp));

        auto colliderComp = std::make_unique<OBBColliderComponent>(bullet.get());
        colliderComp->SetOnEnter([ptr = bullet.get()](GameObject* other) {
            if (other->GetTag() == "PistolEnemy" || other->GetTag() == "AssaultEnemy" || other->GetTag() == "ShotgunEnemy")
                ptr->SetActive(false);
                                 });
        bullet->AddComponent("OBBCollider", std::move(colliderComp));

        bullets_.push_back(std::move(bullet));
    }
}

void ShotgunComponent::FireBullets(GameObject* owner, const Vector3& targetPosition)
{
    Vector3 startPos = owner->GetPosition();
    Vector3 baseDir = Vector3::Normalize(targetPosition - startPos);
    baseDir.y = 0.0f;
    float baseAngle = atan2f(baseDir.x, baseDir.z);

    // 乱数生成器の用意
    static std::random_device rd;
    static std::mt19937 gen(rd());
    // ばらけ角度の範囲（例: ±spreadAngle_度）
    std::uniform_real_distribution<float> angleDist(-spreadAngle_, spreadAngle_);
    // Y方向の微小ばらけ（上下にも少し散らす場合）
    std::uniform_real_distribution<float> yDist(-0.05f, 0.05f);

    for (int i = 0; i < pelletCount_; ++i)
    {
        // ランダムな角度オフセット（度→ラジアン変換）
        float angleOffset = angleDist(gen) * 3.14159265f / 180.0f;
        float angle = baseAngle + angleOffset;
        float yOffset = yDist(gen);

        Vector3 dir = { sinf(angle), yOffset, cosf(angle) };
        dir = Vector3::Normalize(dir);

        auto bullet = std::make_unique<Bullet>("Bullet");
        bullet->Initialize(object3dCommon_, lightManager_, startPos);
        bullet->SetModel("cube.obj");
        bullet->SetRotation({ 0.0f, angle, 0.0f });
        bullet->SetScale(Vector3(0.3f, 0.3f, 1.0f));

        auto bulletComp = std::make_unique<BulletComponent>();
        bulletComp->Initialize(dir, 20.0f, 1.0f);
        bullet->AddComponent("Bullet", std::move(bulletComp));

        auto colliderComp = std::make_unique<OBBColliderComponent>(bullet.get());
        colliderComp->SetOnEnter([ptr = bullet.get()](GameObject* other) {
            if (other->GetTag() == "Player")
                ptr->SetActive(false);
                                 });
        bullet->AddComponent("OBBCollider", std::move(colliderComp));

        bullets_.push_back(std::move(bullet));
    }
}

void ShotgunComponent::StartReload()
{
    isReloading_ = true;
    reloadTimer_ = 0.0f;
}

void ShotgunComponent::Reload(float deltaTime)
{
    reloadTimer_ += deltaTime;
    if (reloadTimer_ >= reloadTime_)
    {
        currentAmmo_ = maxAmmo_;
        isReloading_ = false;
    }
}
