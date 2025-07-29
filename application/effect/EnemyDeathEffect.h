#pragma once
#include <memory>
// math
#include "math/Vector3.h"
// effects
#include "effects/particle/ParticleEmitter.h"

// 敵死亡時のエフェクト管理クラス
class EnemyDeathEffect
{
public:
    // エフェクトタイプの列挙
    enum class EffectType
    {
        Normal,     // 通常の死亡エフェクト
        Explosive,  // 爆発的な死亡エフェクト
        Electric,   // 電撃系の死亡エフェクト
        Dissolve    // 消滅系の死亡エフェクト
    };

    EnemyDeathEffect();
    ~EnemyDeathEffect();

    // 初期化関数
    void Initialize();

    // エフェクトの再生
    void PlayDeathEffect(const Vector3& position, EffectType type = EffectType::Normal);

    // 爆発エフェクトの再生
    void PlayExplosionEffect(const Vector3& position, float scale = 1.0f);

    // 電撃エフェクトの再生
    void PlayElectricEffect(const Vector3& position);

    // 消滅エフェクトの再生
    void PlayDissolveEffect(const Vector3& position);

private:
    // 各種エミッターの初期化
    void InitializeBloodEmitter();
    void InitializeFragmentEmitter();
    void InitializeExplosionEmitter();
    void InitializeElectricEmitter();
    void InitializeDissolveEmitter();
    void InitializeSmokeEmitter();

private:
    // パーティクルエミッター群
    std::unique_ptr<ParticleEmitter> bloodEmitter_;      // 血飛沫エミッター
    std::unique_ptr<ParticleEmitter> fragmentEmitter_;   // 破片エミッター
    std::unique_ptr<ParticleEmitter> explosionEmitter_;  // 爆発エミッター
    std::unique_ptr<ParticleEmitter> electricEmitter_;   // 電撃エミッター
    std::unique_ptr<ParticleEmitter> dissolveEmitter_;   // 消滅エミッター
    std::unique_ptr<ParticleEmitter> smokeEmitter_;      // 煙エミッター

    // エフェクト用テクスチャパス
    const std::string bloodTexturePath_ = "./Resources/circle2.png";
    const std::string fragmentTexturePath_ = "./Resources/circle2.png";
    const std::string explosionTexturePath_ = "./Resources/circle2.png";
    const std::string electricTexturePath_ = "./Resources/circle2.png";
    const std::string dissolveTexturePath_ = "./Resources/circle2.png";
    const std::string smokeTexturePath_ = "./Resources/circle2.png";
};