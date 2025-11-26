#include "RandomInitialVelocityComponent.h"

RandomInitialVelocityComponent::RandomInitialVelocityComponent(const Vector3& minV, const Vector3& maxV)
    : minVelocity_(minV), maxVelocity_(maxV)
{
}

void RandomInitialVelocityComponent::Update(Particle& particle)
{
    // 初回更新時のみ速度を設定（2回目以降は何もしない）
    if (!initialized_)
    {
        // 各軸ごとにランダムな速度を設定
        particle.velocity.x = RandomFloat(minVelocity_.x, maxVelocity_.x);
        particle.velocity.y = RandomFloat(minVelocity_.y, maxVelocity_.y);
        particle.velocity.z = RandomFloat(minVelocity_.z, maxVelocity_.z);
        // 初期化完了フラグを設定
        initialized_ = true;
    }
}

float RandomInitialVelocityComponent::RandomFloat(float min, float max)
{
    // 0.0〜1.0の乱数を生成
    float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    // min〜maxの範囲にスケーリング
    return min + r * (max - min);
}
