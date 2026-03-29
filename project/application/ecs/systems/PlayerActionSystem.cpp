#include "PlayerActionSystem.h"

// engine
#include "engine/ecs/Registry.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/TagComponent.h" // ecs::EcsTagComponent
#include "engine/ecs/components/ColliderComponent.h"
#include "engine/ecs/components/CollisionResponseComponent.h"
#include "engine/ecs/components/InstancedRenderComponent.h"
#include "engine/time/TimeManager.h"

// app components
#include "application/ecs/components/PlayerProgressionComponent.h"
#include "application/ecs/components/SkillComponent.h"
#include "application/ecs/components/DodgeComponent.h"
#include "application/ecs/components/StatusComponent.h"
#include "application/ecs/components/ProjectileComponent.h"
#include "application/ecs/CollisionConfig.h"

// app systems/managers
#include "input/Input.h"
#include "math/MathUtils.h"
#include "math/MatrixFunc.h"
#include "manager/scene/CameraManager.h"
#include "base/Camera.h"
#include "base/WinApp.h"
#include "application/effect/BulletTrailManager.h"
#include <algorithm>
#include <cmath>

void PlayerActionSystem::Update(Registry& registry)
{
    // プレイヤーエンティティを取得 (ecs::EcsTagComponent::Type::Player で探す)
    auto tagView = registry.View<ecs::EcsTagComponent>();
    if (!tagView) return;

    float dt = TimeManager::GetInstance().GetGameContext().deltaTime;
    if (dt <= 0.0f) dt = 0.0166f;

    for (uint32_t i = 0; i < tagView->GetSize(); ++i)
    {
        if (tagView->GetDataFromDenseIndex(i).type != ecs::EcsTagComponent::Type::Player) continue;

        EntityID entity = tagView->GetEntityFromDenseIndex(i);
        
        // 必要なコンポーネントが揃っているか確認
        if (!registry.HasComponent<DodgeComponent>(entity)) continue;

        UpdateDodge(entity, registry, dt);
        
        // 回避中なら移動入力を受け付けない
        auto& dodge = registry.GetComponent<DodgeComponent>(entity);
        if (!dodge.isDodging_)
        {
            if (registry.HasComponent<TransformComponent>(entity) && 
                registry.HasComponent<ecs::StatusComponent>(entity))
            {
                UpdateMovement(entity, registry, dt);
            }
            
            if (registry.HasComponent<SkillComponent>(entity))
            {
                UpdateSkills(entity, registry, dt);
            }
        }
    }
}

void PlayerActionSystem::UpdateDodge(EntityID entity, Registry& registry, float dt)
{
    auto& dodge = registry.GetComponent<DodgeComponent>(entity);
    auto& trans = registry.GetComponent<TransformComponent>(entity);

    if (dodge.cooldownTimer_ > 0.0f) dodge.cooldownTimer_ -= dt;

    if (dodge.isDodging_)
    {
        dodge.timer_ -= dt;
        float progress = 1.0f - ((std::max)(0.0f, dodge.timer_) / dodge.kDuration);
        
        // イージングを用いた補間
        trans.localPosition_ = MathUtils::Lerp(dodge.startPosition_, dodge.targetPosition_, progress);
        trans.isDirty_ = true;

        if (dodge.timer_ <= 0.0f)
        {
            dodge.isDodging_ = false;
            dodge.cooldownTimer_ = dodge.kCooldown;
        }
    }
    else
    {
        // 回避入力
        if (Input::GetInstance()->TriggerKey(DIK_SPACE) && dodge.cooldownTimer_ <= 0.0f)
        {
            auto* input = Input::GetInstance();
            Vector3 inputDir = { 0, 0, 0 };
            if (input->PushKey(DIK_W)) inputDir.z += 1.0f;
            if (input->PushKey(DIK_S)) inputDir.z -= 1.0f;
            if (input->PushKey(DIK_A)) inputDir.x -= 1.0f;
            if (input->PushKey(DIK_D)) inputDir.x += 1.0f;

            if (inputDir.LengthSquared() > 0.01f)
            {
                if (cameraManager_)
                {
                    float yaw = cameraManager_->GetActiveCamera()->GetRotate().y;
                    Vector3 forward(sin(yaw), 0, cos(yaw));
                    Vector3 right(cos(yaw), 0, -sin(yaw));
                    dodge.direction_ = (forward * inputDir.z + right * inputDir.x).Normalize();
                }
                else
                {
                    dodge.direction_ = inputDir.Normalize();
                }
            }
            else
            {
                // 入力がない場合は現在の向き
                float yaw = trans.localRotation_.y;
                dodge.direction_ = { sin(yaw), 0, cos(yaw) };
            }

            dodge.isDodging_ = true;
            dodge.timer_ = dodge.kDuration;
            dodge.startPosition_ = trans.localPosition_;
            dodge.targetPosition_ = dodge.startPosition_ + dodge.direction_ * dodge.kDistance;
        }
    }
}

void PlayerActionSystem::UpdateMovement(EntityID entity, Registry& registry, float dt)
{
    auto* input = Input::GetInstance();
    auto& trans = registry.GetComponent<TransformComponent>(entity);
    auto& status = registry.GetComponent<ecs::StatusComponent>(entity);

    Vector3 moveInput = { 0, 0, 0 };
    if (input->PushKey(DIK_W)) moveInput.z += 1.0f;
    if (input->PushKey(DIK_S)) moveInput.z -= 1.0f;
    if (input->PushKey(DIK_A)) moveInput.x -= 1.0f;
    if (input->PushKey(DIK_D)) moveInput.x += 1.0f;

    if (moveInput.LengthSquared() > 0.01f)
    {
        Vector3 finalDir = moveInput.Normalize();
        if (cameraManager_)
        {
            float yaw = cameraManager_->GetActiveCamera()->GetRotate().y;
            Vector3 forward(sin(yaw), 0, cos(yaw));
            Vector3 right(cos(yaw), 0, -sin(yaw));
            finalDir = (forward * moveInput.z + right * moveInput.x).Normalize();
        }

        float speed = status.moveSpeed_.GetValue();
        trans.localPosition_ = trans.localPosition_ + finalDir * speed * dt;
        trans.isDirty_ = true;
    }

    // マウス方向への回転
    if (cameraManager_)
    {
        Camera* camera = cameraManager_->GetActiveCamera();
        float mouseX = input->GetMouseX();
        float mouseY = input->GetMouseY();

        Matrix4x4 matViewport = MakeViewportMatrix(0, 0, WinApp::kClientWidth, WinApp::kClientHeight, 0, 1);
        Matrix4x4 matVPV = (camera->GetViewMatrix() * camera->GetProjectionMatrix()) * matViewport;
        Matrix4x4 matInverseVPV = Inverse(matVPV);

        Vector3 posNear = MathUtils::Transform({ mouseX, mouseY, 0.0f }, matInverseVPV);
        Vector3 posFar = MathUtils::Transform({ mouseX, mouseY, 1.0f }, matInverseVPV);

        Vector3 rayDir = (posFar - posNear).Normalize();
        if (std::abs(rayDir.y) > 0.0001f)
        {
            float t = (trans.localPosition_.y - posNear.y) / rayDir.y;
            Vector3 targetPos = posNear + rayDir * t;
            Vector3 lookDir = (targetPos - trans.localPosition_);
            lookDir.y = 0.0f;

            if (lookDir.LengthSquared() > 0.01f)
            {
                float targetYaw = atan2f(lookDir.x, lookDir.z);
                trans.localRotation_.y = MathUtils::LerpAngle(trans.localRotation_.y, targetYaw, 0.2f);
                trans.isDirty_ = true;
            }
        }
    }
}

void PlayerActionSystem::UpdateSkills(EntityID entity, Registry& registry, float dt)
{
    auto& skill = registry.GetComponent<SkillComponent>(entity);
    
    // タイマー更新
    if (skill.lmbTimer_ > 0.0f) skill.lmbTimer_ -= dt;
    if (skill.rmbTimer_ > 0.0f) skill.rmbTimer_ -= dt;
    if (skill.decoyTimer_ > 0.0f) skill.decoyTimer_ -= dt;
    if (skill.impactTimer_ > 0.0f) skill.impactTimer_ -= dt;
    if (skill.beamTimer_ > 0.0f) skill.beamTimer_ -= dt;

    UpdateLMB(entity, registry, dt);
    UpdateRMB(entity, registry, dt);
    UpdateQ(entity, registry, dt);
    UpdateE(entity, registry, dt);
    UpdateR(entity, registry, dt);
}

void PlayerActionSystem::UpdateLMB(EntityID entity, Registry& registry, float)
{
    auto& skill = registry.GetComponent<SkillComponent>(entity);
    if (!skill.isLmbUnlocked_) return;
    if (skill.lmbTimer_ > 0.0f) return;

    if (Input::GetInstance()->IsMouseButtonPressed(0))
    {
        // 弾を生成
        auto& trans = registry.GetComponent<TransformComponent>(entity);
        float yaw = trans.localRotation_.y;
        Vector3 dir = { sin(yaw), 0, cos(yaw) };
        
        EntityID proj = registry.CreateEntity();
        Vector3 spawnPos = { trans.localPosition_.x, 0.5f, trans.localPosition_.z };
        registry.AddComponent<TransformComponent>(proj, { spawnPos, trans.localRotation_, {1,1,1} });
        
        ProjectileComponent pc;
        pc.type_ = ProjectileComponent::Type::Lmb;
        pc.velocity_ = dir * 80.0f;
        pc.damage_ = 10.0f;
        pc.lifetime_ = 1.5f;
        pc.trailId_ = BulletTrailManager::GetInstance().RegisterBulletManual();
        registry.AddComponent<ProjectileComponent>(proj, pc);

        // Rendering
        InstancedRenderComponent render;
        render.modelName_ = "bullet";
        render.useInstancing_ = true;
        registry.AddComponent<InstancedRenderComponent>(proj, render);

        // コライダー設定 (BNS-Style: 振る舞いをデータとして持たせる)
        ColliderComponent col;
        col.type_ = ColliderType::Sphere;
        col.sphere_.radius = 0.2f;
        col.previousPosition_ = trans.localPosition_;
        col.isTrigger_ = true; // 物理的に押し返さない

        // フィルタリング設定
        col.layer = CollisionLayer::PlayerBullet;
        col.mask = CollisionLayer::Enemy | CollisionLayer::Obstacle;

        // 衝突応答
        col.onCollisionEnter = [&registry, proj](const CollisionPartnerInfo& other) {
            // 弾は Enemy または Obstacle に当たったら自身を消す
            if (registry.HasComponent<ColliderComponent>(other.entity)) {
                auto& otherCol = registry.GetComponent<ColliderComponent>(other.entity);
                if (otherCol.layer & (CollisionLayer::Enemy | CollisionLayer::Obstacle)) {
                    registry.DestroyEntityDeferred(proj);
                }
            }
        };

        registry.AddComponent<ColliderComponent>(proj, col);
        registry.AddComponent<CollisionResponseComponent>(proj, {});

        skill.lmbTimer_ = skill.kLmbCooldown;
    }
}

void PlayerActionSystem::UpdateRMB(EntityID entity, Registry& registry, float)
{
    auto& skill = registry.GetComponent<SkillComponent>(entity);
    if (!skill.isRmbUnlocked_) return;
    // 実装予定
}

void PlayerActionSystem::UpdateQ(EntityID entity, Registry& registry, float)
{
    auto& skill = registry.GetComponent<SkillComponent>(entity);
    if (!skill.isDecoyUnlocked_) return;
    // 実装予定
}

void PlayerActionSystem::UpdateE(EntityID entity, Registry& registry, float)
{
    auto& skill = registry.GetComponent<SkillComponent>(entity);
    if (!skill.isImpactUnlocked_) return;
    // 実装予定
}

void PlayerActionSystem::UpdateR(EntityID entity, Registry& registry, float)
{
    auto& skill = registry.GetComponent<SkillComponent>(entity);
    if (!skill.isBeamUnlocked_) return;
    // 実装予定
}
