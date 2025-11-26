#include "ScaleOverLifetimeComponent.h"

// 生存時間比率の最大値（100%）
constexpr float kMaxLifeRatio = 1.0f;

ScaleOverLifetimeComponent::ScaleOverLifetimeComponent(float start, float end)
    : startScale_(start), endScale_(end)
{
}

void ScaleOverLifetimeComponent::Update(Particle& particle)
{
    // 生存時間の割合を計算
    float lifeRatio = particle.currentTime / particle.lifeTime;
    
    // 最大値でクランプ
    if (lifeRatio > kMaxLifeRatio) lifeRatio = kMaxLifeRatio;
    
    // 開始スケールから終了スケールへ線形補間
    float scale = startScale_ + (endScale_ - startScale_) * lifeRatio;
    
    // 3軸すべてに同じスケールを適用
    particle.transform.scale = Vector3(scale, scale, scale);
}
