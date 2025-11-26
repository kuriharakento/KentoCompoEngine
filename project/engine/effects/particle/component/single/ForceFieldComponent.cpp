#include "ForceFieldComponent.h"

// ゼロ除算を回避するための閾値（距離の二乗がこの値以下の場合は力を適用しない）
constexpr float kZeroDivisionThreshold = 0.0001f;
// 斥力の場合に適用する係数（力の方向を逆転）
constexpr float kRepelMultiplier = -1.0f;

ForceFieldComponent::ForceFieldComponent(const Vector3& center, float strength, float maxDistance, ForceType type)
    : forceCenter(center)
    , strength(strength)
    , maxDistance(maxDistance)
    , type(type)
{
}

void ForceFieldComponent::Update(Particle& particle)
{
    // パーティクルから力場の中心への方向ベクトルを計算
    Vector3 direction = forceCenter - particle.transform.translate;
    // 距離の二乗を計算（平方根の計算を避けて効率化）
    float distanceSq = direction.LengthSquared();

    // 最大距離内かつゼロ除算を回避できる距離の場合のみ力を適用
    if (distanceSq < maxDistance * maxDistance && distanceSq > kZeroDivisionThreshold)
    {
        // 実際の距離を計算
        float distance = std::sqrt(distanceSq);
        // 距離が離れるほど力が弱まる（距離に反比例）
        float forceMagnitude = strength / distance;

        // 力の方向を正規化
        direction.NormalizeSelf();

        // 斥力の場合は力の方向を逆転
        if (type == ForceType::Repel)
        {
            forceMagnitude *= kRepelMultiplier;
        }

        // 計算した力を速度に加算
        particle.velocity += direction * forceMagnitude;
    }
}