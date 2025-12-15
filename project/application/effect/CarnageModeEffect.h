#pragma once
#include <memory>
#include "math/Vector3.h"
#include "effects/particle/ParticleEmitter.h"

/**
 * @brief カーネイジモード発動時のビジュアルエフェクトクラス
 * 
 * プレイヤーの強化状態を視覚的に表現するための複合エフェクトです。
 * オーラ、煙、軌跡、稲妻、バーストの5種類のエミッターを組み合わせて
 * 迫力のある演出を実現します。
 */
class CarnageModeEffect
{
public:
    CarnageModeEffect();
    ~CarnageModeEffect();

	/**
	 * @brief エフェクトシステムの初期化
	 * 
	 * 5種類のパーティクルエミッター（オーラ、煙、軌跡、稲妻、バースト）を
	 * 作成し、それぞれに適切なモジュールを設定します。
	 */
    void Initialize();
    
    /**
     * @brief オーラエフェクトの再生
     * 
     * プレイヤーの周囲に赤いオーラと煙を表示します。
     * 
     * @param position エフェクトを表示する位置
     */
    void PlayAuraEffect(const Vector3& position);
    
    /**
     * @brief 軌跡エフェクトの再生
     * 
     * プレイヤーの移動に追従する赤い軌跡と稲妻を表示します。
     * 
     * @param position エフェクトを表示する位置
     * @param direction 移動方向（現在未使用）
     */
    void PlayTrailEffect(const Vector3& position, const Vector3& direction);
    
    /**
     * @brief 終了エフェクトの再生
     * 
     * カーネイジモード終了時にバーストエフェクトを表示します。
     * 
     * @param position エフェクトを表示する位置
     */
    void PlayEndEffect(const Vector3& position);
    
    /**
     * @brief エフェクトの更新
     * 
     * @param deltaTime デルタタイム（現在未使用、ParticleManagerが更新を担当）
     */
    void Update(float deltaTime);

private:
    const std::string auraTexturePath_ = "./Resources/gradation.png";         ///< オーラ用テクスチャパス
    const std::string smokeTexturePath_ = "./Resources/circle2.png";          ///< 煙用テクスチャパス
    const std::string trailTexturePath_ = "./Resources/gradation.png";        ///< 軌跡用テクスチャパス
    const std::string lightningTexturePath_ = "./Resources/star.png";         ///< 稲妻用テクスチャパス
    const std::string burstTexturePath_ = "./Resources/gradation.png";        ///< バースト用テクスチャパス
};
