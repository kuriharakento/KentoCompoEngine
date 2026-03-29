#include "PlayerSystem.h"
#include "engine/ecs/Registry.h"
#include "input/Input.h"
#include "engine/ecs/components/MovementComponent.h"
#include "application/ecs/components/PlayerComponent.h"
#include "application/ecs/components/StatusComponent.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/ColliderComponent.h"

#include "math/MathUtils.h"
#include "math/MatrixFunc.h"
#include "base/Camera.h"
#include "manager/scene/CameraManager.h"
#include "base/WinApp.h"
#include "engine/time/TimeManager.h"
#include <algorithm>

void PlayerSystem::Update(Registry& registry)
{
    auto view = registry.View<PlayerComponent>();
    if (!view) return;

    auto* input = Input::GetInstance();
    // プレイヤーは realDeltaTime を使用することが多いため、そちらを優先
    float deltaTime = TimeManager::GetInstance().GetGameContext().realDeltaTime;
    if (deltaTime <= 0.0f) deltaTime = 0.0166f;

    Camera* camera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
    
    for (uint32_t i = 0; i < view->GetSize(); ++i)
    {
        EntityID entity = view->GetEntityFromDenseIndex(i);
        if (!registry.HasComponent<ecs::StatusComponent>(entity)) continue;
        if (!registry.HasComponent<TransformComponent>(entity)) continue;

        PlayerComponent& pRef = view->GetDataFromDenseIndex(i);
        ecs::StatusComponent& sRef = registry.GetComponent<ecs::StatusComponent>(entity);
        TransformComponent& tRef = registry.GetComponent<TransformComponent>(entity);

        // --- 回避（Dodge）タイマー更新 ---
        if (pRef.dodgeCooldownTimer_ > 0.0f) pRef.dodgeCooldownTimer_ -= deltaTime;

        if (pRef.isDodging_)
        {
            pRef.dodgeTimer_ -= deltaTime;
            float progress = 1.0f - ((std::max)(0.0f, pRef.dodgeTimer_) / pRef.dodgeDuration_);
            
            // イージングを伴う座標補間
            tRef.localPosition_ = MathUtils::Lerp(pRef.dodgeStartPosition_, pRef.dodgeTargetPosition_, progress);
            tRef.isDirty_ = true;

            if (pRef.dodgeTimer_ <= 0.0f)
            {
                pRef.isDodging_ = false;
                pRef.dodgeCooldownTimer_ = pRef.dodgeCooldown_;
            }
            continue; // 回避中は他の移動入力を受け付けない
        }

        // --- 会避入力チェック ---
        if (input->TriggerKey(DIK_SPACE) && pRef.dodgeCooldownTimer_ <= 0.0f)
        {
            Vector3 inputDir = { 0, 0, 0 };
            if (input->PushKey(DIK_W)) inputDir.z += 1.0f;
            if (input->PushKey(DIK_S)) inputDir.z -= 1.0f;
            if (input->PushKey(DIK_A)) inputDir.x -= 1.0f;
            if (input->PushKey(DIK_D)) inputDir.x += 1.0f;

            if (inputDir.LengthSquared() > 0.01f)
            {
                if (camera)
                {
                    float yaw = camera->GetRotate().y;
                    Vector3 forward(sin(yaw), 0, cos(yaw));
                    Vector3 right(cos(yaw), 0, -sin(yaw));
                    pRef.dodgeDirection_ = (forward * inputDir.z + right * inputDir.x).Normalize();
                }
                else
                {
                    pRef.dodgeDirection_ = inputDir.Normalize();
                }
            }
            else
            {
                // 入力がない場合は現在の向きに回避
                float yaw = tRef.localRotation_.y;
                pRef.dodgeDirection_ = { sin(yaw), 0, cos(yaw) };
            }

            pRef.isDodging_ = true;
            pRef.dodgeTimer_ = pRef.dodgeDuration_;
            pRef.dodgeStartPosition_ = tRef.localPosition_;
            pRef.dodgeTargetPosition_ = pRef.dodgeStartPosition_ + pRef.dodgeDirection_ * pRef.dodgeDistance_;
            pRef.invincibleTimer_ = pRef.dodgeInvincibleTime_;
            pRef.isInvincible_ = true;
            if (registry.HasComponent<ecs::ColliderComponent>(entity))
            {
                // CCD判定用に現在(ダッシュ開始時)のワールド位置を記録
                auto& coll = registry.GetComponent<ecs::ColliderComponent>(entity);
                coll.previousPosition_ = MathUtils::GetTranslateFromMatrix(tRef.worldMatrix_);
            }
            continue;
        }

        // --- 通常移動入力 ---
        Vector3 moveInput = { 0, 0, 0 };
        if (input->PushKey(DIK_W)) moveInput.z += 1.0f;
        if (input->PushKey(DIK_S)) moveInput.z -= 1.0f;
        if (input->PushKey(DIK_A)) moveInput.x -= 1.0f;
        if (input->PushKey(DIK_D)) moveInput.x += 1.0f;

        pRef.hasMovementInput_ = (moveInput.LengthSquared() > 0.01f);
        if (pRef.hasMovementInput_)
        {
            Vector3 finalDir = moveInput.Normalize();
            if (camera)
            {
                float yaw = camera->GetRotate().y;
                Vector3 forward(sin(yaw), 0, cos(yaw));
                Vector3 right(cos(yaw), 0, -sin(yaw));
                finalDir = (forward * moveInput.z + right * moveInput.x).Normalize();
            }

            // スナッピーな挙動のため座標を直接更新
            float speed = sRef.moveSpeed_.GetValue() * pRef.moveSpeedMultiplier_;
            tRef.localPosition_ = tRef.localPosition_ + finalDir * speed * deltaTime;
            tRef.isDirty_ = true;

            // MovementComponent の velocity も同期（物理演算等での参照用）
            if (registry.HasComponent<MovementComponent>(entity))
            {
                registry.GetComponent<MovementComponent>(entity).velocity_ = finalDir * speed;
            }
        }
        else
        {
            if (registry.HasComponent<MovementComponent>(entity))
            {
                registry.GetComponent<MovementComponent>(entity).velocity_ = { 0, 0, 0 };
            }
        }

        // --- マウス方向への回転（Look At Mouse） ---
        if (camera)
        {
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
                float t = (tRef.localPosition_.y - posNear.y) / rayDir.y;
                Vector3 targetPos = posNear + rayDir * t;
                Vector3 lookDir = (targetPos - tRef.localPosition_);
                lookDir.y = 0.0f;

                if (lookDir.LengthSquared() > 0.01f)
                {
                    float targetYaw = atan2f(lookDir.x, lookDir.z);
                    // なめらかな回転補間（kDodgeRotationInterpolation 相当の 0.2f を使用）
                    tRef.localRotation_.y = MathUtils::LerpAngle(tRef.localRotation_.y, targetYaw, 0.2f);
                    tRef.isDirty_ = true;
                }
            }
        }

        // --- 無敵タイマー更新 ---
        if (pRef.invincibleTimer_ > 0.0f)
        {
            pRef.invincibleTimer_ -= deltaTime;
            if (pRef.invincibleTimer_ <= 0.0f)
            {
                pRef.invincibleTimer_ = 0.0f;
                pRef.isInvincible_ = false;
            }
        }
    }
}
