#include "MovementSystem.h"
#include "engine/ecs/Registry.h"
#include "engine/time/TimeManager.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/MovementComponent.h"

void MovementSystem::Update(Registry& registry)
{
    float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;
    if (deltaTime <= 0.0f) return;

    auto view = registry.View<MovementComponent>();
    if (!view) return;

    for (uint32_t i = 0; i < view->GetSize(); ++i)
    {
        EntityID entity = view->GetEntityFromDenseIndex(i);
        if (!registry.HasComponent<TransformComponent>(entity)) continue;

        auto& move = view->GetData(entity);
        auto& transform = registry.GetComponent<TransformComponent>(entity);

        // 加速度を速度に適用
        move.velocity_.x += move.acceleration_.x * deltaTime;
        move.velocity_.y += move.acceleration_.y * deltaTime;
        move.velocity_.z += move.acceleration_.z * deltaTime;

        // 摩擦（簡易的なドラッグ）
        move.velocity_.x *= std::pow(move.friction_, deltaTime * 60.0f);
        move.velocity_.y *= std::pow(move.friction_, deltaTime * 60.0f);
        move.velocity_.z *= std::pow(move.friction_, deltaTime * 60.0f);

        // 重力適用
        if (move.useGravity_ && !move.isGrounded_)
        {
            move.velocity_.y -= move.gravity_ * deltaTime;
        }

        // 最高速度制限
        float speedSq = move.velocity_.LengthSquared();
        if (speedSq > move.maxSpeed_ * move.maxSpeed_)
        {
            float speed = std::sqrt(speedSq);
            move.velocity_.x = (move.velocity_.x / speed) * move.maxSpeed_;
            move.velocity_.y = (move.velocity_.y / speed) * move.maxSpeed_;
            move.velocity_.z = (move.velocity_.z / speed) * move.maxSpeed_;
        }

        // 座標に適用
        transform.localPosition_.x += move.velocity_.x * deltaTime;
        transform.localPosition_.y += move.velocity_.y * deltaTime;
        transform.localPosition_.z += move.velocity_.z * deltaTime;

        // 地面接地（簡易的な Y=0 制限）
        if (move.useGravity_ && transform.localPosition_.y < 1.0f)
        {
            transform.localPosition_.y = 1.0f;
            move.velocity_.y = 0.0f;
            move.isGrounded_ = true;
        }

        // 加速度をリセット（外部から毎フレーム加算される想定）
        move.acceleration_ = { 0.0f, 0.0f, 0.0f };
        
        // 座標が変わったので Dirty フラグを立てる
        transform.isDirty_ = true;
    }
}
