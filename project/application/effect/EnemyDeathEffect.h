#pragma once
#include <memory>
#include "math/Vector3.h"
#include "effects/particle/ParticleEmitter.h"

/**
 * @brief 謨ｵ豁ｻ莠｡譎ゅ・繝薙ず繝･繧｢繝ｫ繧ｨ繝輔ぉ繧ｯ繝育ｮ｡逅・け繝ｩ繧ｹ・・PS迚茨ｼ・
 */
class EnemyDeathEffect
{
public:
    enum class EffectType
    {
        Normal,
        Explosive,
        Electric,
        Dissolve
    };

    EnemyDeathEffect();
    ~EnemyDeathEffect();

    void Initialize();
    void PlayDeathEffect(const Vector3& position, EffectType type = EffectType::Normal);
    void PlayExplosionEffect(const Vector3& position, float scale = 1.0f);
    void PlayElectricEffect(const Vector3& position);
    void PlayDissolveEffect(const Vector3& position);

private:
    void InitializeBloodEmitter();
    void InitializeFragmentEmitter();
    void InitializeExplosionEmitter();
    void InitializeElectricEmitter();
    void InitializeDissolveEmitter();
    void InitializeSmokeEmitter();

private:
    const std::string bloodTexturePath_ = "./Resources/circle2.png";
    const std::string fragmentTexturePath_ = "./Resources/circle2.png";
    const std::string explosionTexturePath_ = "./Resources/circle2.png";
    const std::string electricTexturePath_ = "./Resources/circle2.png";
    const std::string dissolveTexturePath_ = "./Resources/circle2.png";
    const std::string smokeTexturePath_ = "./Resources/circle2.png";
};
