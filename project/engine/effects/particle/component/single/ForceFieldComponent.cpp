#include "ForceFieldComponent.h"

ForceFieldComponent::ForceFieldComponent(const Vector3& center, float strength, float maxDistance, ForceType type)
    : forceCenter(center)
    , strength(strength)
    , maxDistance(maxDistance)
    , type(type)
{
}

void ForceFieldComponent::Update(Particle& particle)
{
    Vector3 direction = forceCenter - particle.transform.translate;
    float distanceSq = direction.LengthSquared(); // 距離の二乗

    if (distanceSq < maxDistance * maxDistance && distanceSq > 0.0001f) // 0除算回避
    {
        float distance = std::sqrt(distanceSq);
        // 距離が離れるほど力が弱まる（逆二乗の法則など）
        // ここでは単純に距離に反比例させる
        float forceMagnitude = strength / distance;

        // 力の方向を正規化
        direction.NormalizeSelf();

        if (type == ForceType::Repel)
        {
            forceMagnitude *= -1.0f; // 斥力の場合は逆方向
        }

        // 力を速度に加算
        particle.velocity += direction * forceMagnitude;
    }
}