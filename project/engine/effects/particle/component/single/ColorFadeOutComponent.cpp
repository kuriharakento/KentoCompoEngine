#include "ColorFadeOutComponent.h"

#include "base/GraphicsTypes.h"

// 寿命比率の最大値（寿命を超えた場合のクランプ値）
constexpr float kMaxLifeRatio = 1.0f;
// 完全不透明時のアルファ値
constexpr float kFullOpacity = 1.0f;

void ColorFadeOutComponent::Update(Particle& particle)
{
    // 寿命比率を計算（0.0 = 生成直後、1.0 = 寿命終了）
    float lifeRatio = particle.currentTime / particle.lifeTime;
    // 寿命を超えた場合は最大値にクランプ
    if (lifeRatio > kMaxLifeRatio) lifeRatio = kMaxLifeRatio;
    // アルファ値を設定（寿命が進むほど透明に）
    particle.color.w = kFullOpacity - lifeRatio;
}
