#pragma once
#include <memory>
#include "math/Vector3.h"
#include "effects/particle/ParticleEmitter.h"

/**
 * @brief 蝗樣∩繧｢繧ｯ繧ｷ繝ｧ繝ｳ譎ゅ・繝代・繝・ぅ繧ｯ繝ｫ繧ｨ繝輔ぉ繧ｯ繝茨ｼ・PS迚茨ｼ・
 */
class DodgeEffectParticle
{
public:
    DodgeEffectParticle();
    ~DodgeEffectParticle();

    void Initialize();
    void PlayEffect(const Vector3& position, const Vector3& direction);
    void CreateAfterImage(const Vector3& position, const Vector3& rotation);
    void PlayFadeOutEffect(const Vector3& position);

private:
    const std::string afterImageTexturePath_ = "./Resources/circle2.png";
    const std::string trailTexturePath_ = "./Resources/gradation.png";
    const std::string burstTexturePath_ = "./Resources/star.png";
};
