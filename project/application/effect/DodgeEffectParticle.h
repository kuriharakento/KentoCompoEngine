#pragma once
#include <memory>
#include "math/Vector3.h"
#include "effects/particle/ParticleEmitter.h"

/**
 * @brief 回避アクション時のパーティクルエフェクトクラス
 * 
 * プレイヤーの回避動作に付随する視覚効果を管理します。
 * 残像、軌跡、バーストの3種類のエミッターを組み合わせて
 * 素早い動きを強調します。
 */
class DodgeEffectParticle
{
public:
    DodgeEffectParticle();
    ~DodgeEffectParticle();

	/**
	 * @brief エフェクトシステムの初期化
	 * 
	 * 残像、軌跡、バーストの3種類のパーティクルエミッターを作成し、
	 * それぞれに適切なモジュールを設定します。
	 */
    void Initialize();
    
    /**
     * @brief 回避エフェクトの再生
     * 
     * 指定位置でバーストと軌跡エフェクトを表示します。
     * 
     * @param position エフェクトを表示する位置
     * @param direction 移動方向（現在未使用）
     */
    void PlayEffect(const Vector3& position, const Vector3& direction);
    
    /**
     * @brief 残像エフェクトの生成
     * 
     * プレイヤーの移動軌跡に残像を配置します。
     * 
     * @param position 残像を配置する位置
     * @param rotation 残像の回転（現在未使用）
     */
    void CreateAfterImage(const Vector3& position, const Vector3& rotation);
    
    /**
     * @brief フェードアウトエフェクトの再生
     * 
     * 軌跡エフェクトを画面外に移動させて停止します。
     * 
     * @param position 基準位置（現在未使用）
     */
    void PlayFadeOutEffect(const Vector3& position);

private:
    const std::string afterImageTexturePath_ = "./Resources/circle2.png";   ///< 残像用テクスチャパス
    const std::string trailTexturePath_ = "./Resources/gradation.png";      ///< 軌跡用テクスチャパス
    const std::string burstTexturePath_ = "./Resources/star.png";           ///< バースト用テクスチャパス
};
