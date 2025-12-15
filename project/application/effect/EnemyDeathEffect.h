#pragma once
#include <memory>
#include "math/Vector3.h"
#include "effects/particle/ParticleEmitter.h"

/**
 * @brief 敵死亡時のビジュアルエフェクト管理クラス
 * 
 * 敵キャラクターが倒された際の視覚効果を管理します。
 * 通常、爆発、電撃、溶解の4種類のエフェクトタイプに対応しています。
 */
class EnemyDeathEffect
{
public:
/**
 * @brief エフェクトのタイプ
 * 
 * 敵の死亡演出のバリエーションを定義します。
 */
    enum class EffectType
    {
        Normal,      ///< 通常の死亡エフェクト（血飛沫、破片、煙）
        Explosive,   ///< 爆発エフェクト
        Electric,    ///< 電撃エフェクト
        Dissolve     ///< 溶解エフェクト
    };

    EnemyDeathEffect();
    ~EnemyDeathEffect();

/**
 * @brief エフェクトシステムの初期化
 * 
 * 全種類のパーティクルエミッター（血、破片、爆発、電撃、溶解、煙）を
 * 作成して登録します。
 */
    void Initialize();
    
    /**
     * @brief 死亡エフェクトの再生
     * 
     * 指定されたタイプに応じた死亡エフェクトを表示します。
     * 
     * @param position エフェクトを表示する位置
     * @param type エフェクトのタイプ（デフォルト: Normal）
     */
    void PlayDeathEffect(const Vector3& position, EffectType type = EffectType::Normal);
    
    /**
     * @brief 爆発エフェクトの再生
     * 
     * 爆発、破片、煙を組み合わせた派手なエフェクトを表示します。
     * 
     * @param position エフェクトを表示する位置
     * @param scale スケール（現在未使用）
     */
    void PlayExplosionEffect(const Vector3& position, float scale = 1.0f);
    
    /**
     * @brief 電撃エフェクトの再生
     * 
     * @param position エフェクトを表示する位置
     */
    void PlayElectricEffect(const Vector3& position);
    
    /**
     * @brief 溶解エフェクトの再生
     * 
     * @param position エフェクトを表示する位置
     */
    void PlayDissolveEffect(const Vector3& position);

private:
/**
 * @brief 血飛沫エミッターの初期化
 */
    void InitializeBloodEmitter();
    
    /**
     * @brief 破片エミッターの初期化
     */
    void InitializeFragmentEmitter();
    
    /**
     * @brief 爆発エミッターの初期化
     */
    void InitializeExplosionEmitter();
    
    /**
     * @brief 電撃エミッターの初期化
     */
    void InitializeElectricEmitter();
    
    /**
     * @brief 溶解エミッターの初期化
     */
    void InitializeDissolveEmitter();
    
    /**
     * @brief 煙エミッターの初期化
     */
    void InitializeSmokeEmitter();

private:
    const std::string bloodTexturePath_ = "./Resources/circle2.png";        ///< 血飛沫用テクスチャパス
    const std::string fragmentTexturePath_ = "./Resources/circle2.png";     ///< 破片用テクスチャパス
    const std::string explosionTexturePath_ = "./Resources/circle2.png";    ///< 爆発用テクスチャパス
    const std::string electricTexturePath_ = "./Resources/circle2.png";     ///< 電撃用テクスチャパス
    const std::string dissolveTexturePath_ = "./Resources/circle2.png";     ///< 溶解用テクスチャパス
    const std::string smokeTexturePath_ = "./Resources/circle2.png";        ///< 煙用テクスチャパス
};
