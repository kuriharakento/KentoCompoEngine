#pragma once
#include <memory>
#include "math/Vector3.h"
#include "effects/particle/ParticleEmitter.h"

/**
 * @brief カーネイジモード発動時のビジュアルエフェクト！EPS版！E
 */
class CarnageModeEffect
{
public:
    CarnageModeEffect();
    ~CarnageModeEffect();

    void Initialize();
    void PlayAuraEffect(const Vector3& position);
    void PlayTrailEffect(const Vector3& position, const Vector3& direction);
    void PlayEndEffect(const Vector3& position);
    void Update(float deltaTime);

private:
    const std::string auraTexturePath_ = "./Resources/gradation.png";
    const std::string smokeTexturePath_ = "./Resources/circle2.png";
    const std::string trailTexturePath_ = "./Resources/gradation.png";
    const std::string lightningTexturePath_ = "./Resources/star.png";
    const std::string burstTexturePath_ = "./Resources/gradation.png";
};
