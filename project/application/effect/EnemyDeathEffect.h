#pragma once
#include <memory>
// math
#include "math/Vector3.h"
// effects
#include "effects/particle/ParticleEmitter.h"

/**
 * @brief 敵死亡時のビジュアルエフェクト管理クラス
 * 
 * 敵キャラクターが倒されたときに再生される、多様な視覚効果を管理します。
 * 血飛沫、破片、爆発、電撃、消滅など、複数のエフェクトタイプに対応し、
 * 敵の種類や状況に応じて異なる演出を提供します。
 * 
 * 主な機能:
 * - 4種類の死亡エフェクトタイプ（通常、爆発、電撃、消滅）
 * - 物理的な動きを持つパーティクル（重力、バウンド、空気抵抗）
 * - スケーラブルな爆発エフェクト
 * - 複数エミッターの組み合わせによる複雑な演出
 * 
 * @code
 * // 使用例
 * EnemyDeathEffect deathEffect;
 * deathEffect.Initialize();
 * deathEffect.PlayDeathEffect(enemyPos, EnemyDeathEffect::EffectType::Explosive);
 * @endcode
 */
class EnemyDeathEffect
{
public:
    /**
     * @brief 死亡エフェクトの種類
     * 
     * 敵の種類や倒し方に応じて異なるエフェクトを選択できます。
     */
    enum class EffectType
    {
        Normal,     ///< 通常の死亡エフェクト（血飛沫、破片、煙）
        Explosive,  ///< 爆発的な死亡エフェクト（炎、大量の破片）
        Electric,   ///< 電撃系の死亡エフェクト（青白い放電）
        Dissolve    ///< 消滅系の死亡エフェクト（紫色の粒子で消滅）
    };

    EnemyDeathEffect();
    ~EnemyDeathEffect();

    /**
     * @brief エフェクトシステムの初期化
     * 
     * 全てのパーティクルエミッターを初期化します。
     * 各エフェクトタイプに対応したエミッターの設定を行います。
     */
    void Initialize();

    /**
     * @brief 死亡エフェクトの再生
     * 
     * 指定されたタイプの死亡エフェクトを再生します。
     * 
     * @param position エフェクト発生位置（敵の位置）
     * @param type エフェクトタイプ、デフォルトは通常エフェクト
     */
    void PlayDeathEffect(const Vector3& position, EffectType type = EffectType::Normal);

    /**
     * @brief 爆発エフェクトの再生
     * 
     * スケール調整可能な爆発エフェクトを再生します。
     * 連鎖的な二次爆発も含みます。
     * 
     * @param position エフェクト発生位置
     * @param scale 爆発のスケール（1.0がデフォルト）
     */
    void PlayExplosionEffect(const Vector3& position, float scale = 1.0f);

    /**
     * @brief 電撃エフェクトの再生
     * 
     * 複数の放電ポイントから分岐する電撃エフェクトを再生します。
     * 
     * @param position エフェクト発生位置
     */
    void PlayElectricEffect(const Vector3& position);

    /**
     * @brief 消滅エフェクトの再生
     * 
     * ゆっくりと上昇しながら消えていくエフェクトを再生します。
     * 
     * @param position エフェクト発生位置
     */
    void PlayDissolveEffect(const Vector3& position);

private:
    // 各エミッターの初期化メソッド
    void InitializeBloodEmitter();      ///< 血飛沫エミッターの初期化
    void InitializeFragmentEmitter();   ///< 破片エミッターの初期化
    void InitializeExplosionEmitter();  ///< 爆発エミッターの初期化
    void InitializeElectricEmitter();   ///< 電撃エミッターの初期化
    void InitializeDissolveEmitter();   ///< 消滅エミッターの初期化
    void InitializeSmokeEmitter();      ///< 煙エミッターの初期化

private:
    std::unique_ptr<ParticleEmitter> bloodEmitter_;      ///< 血飛沫エミッター（赤色の液体パーティクル）
    std::unique_ptr<ParticleEmitter> fragmentEmitter_;   ///< 破片エミッター（物理挙動を持つ固体片）
    std::unique_ptr<ParticleEmitter> explosionEmitter_;  ///< 爆発エミッター（オレンジ色の炎）
    std::unique_ptr<ParticleEmitter> electricEmitter_;   ///< 電撃エミッター（青白い放電）
    std::unique_ptr<ParticleEmitter> dissolveEmitter_;   ///< 消滅エミッター（紫色の粒子）
    std::unique_ptr<ParticleEmitter> smokeEmitter_;      ///< 煙エミッター（灰色の煙）

    // エフェクト用テクスチャパス
    const std::string bloodTexturePath_ = "./Resources/circle2.png";
    const std::string fragmentTexturePath_ = "./Resources/circle2.png";
    const std::string explosionTexturePath_ = "./Resources/circle2.png";
    const std::string electricTexturePath_ = "./Resources/circle2.png";
    const std::string dissolveTexturePath_ = "./Resources/circle2.png";
    const std::string smokeTexturePath_ = "./Resources/circle2.png";
};