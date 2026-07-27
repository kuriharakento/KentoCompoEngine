#pragma once

#include <DirectXMath.h>

namespace KCE
{
namespace ecs
{
    /**
     * @brief エンティティの速度を保持するコンポーネント。
     *
     * 毎フレーム TransformMovementSystem（または EnemyBehaviorSystem）が
     * velocity を TransformComponent::localPosition に加算する。
     * 加算は外部のシステムが行い、このコンポーネントは純粋にデータのみを保持する。
     */
    struct VelocityComponent
    {
        // 現在の速度ベクトル（ワールド空間、units/sec）
        DirectX::XMFLOAT3 velocity = { 0.0f, 0.0f, 0.0f };
    };
}
} // namespace KCE
