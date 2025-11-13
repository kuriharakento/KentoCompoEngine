#pragma once
// effects
#include "effects/particle/ParticleEmitter.h"
// math
#include "math/Vector3.h"

/**
 * @brief プレイヤー回避動作時のビジュアルエフェクトクラス
 * 
 * 回避（ダッシュ）時に発生する視覚効果を管理します。
 * 残像、移動軌跡、バーストエフェクトなど複数のパーティクルエフェクトを組み合わせて、
 * 高速移動の印象を与える演出を実現します。
 * 
 * 主な機能:
 * - 回避開始時の爆発的なバーストエフェクト
 * - 高速移動中の軌跡パーティクル
 * - キャラクターの残像効果
 * - 回避終了時のフェードアウト
 * 
 * @code
 * // 使用例
 * DodgeEffectParticle dodgeEffect;
 * dodgeEffect.Initialize();
 * dodgeEffect.PlayEffect(playerPos, playerDirection);  // 回避開始
 * dodgeEffect.CreateAfterImage(playerPos, playerRotation);  // 残像生成
 * dodgeEffect.PlayFadeOutEffect(playerPos);  // 回避終了
 * @endcode
 */
class DodgeEffectParticle
{
public:
    DodgeEffectParticle();
    ~DodgeEffectParticle();

    /**
     * @brief エフェクトシステムの初期化
     * 
     * 各種パーティクルエミッターを作成し、初期パラメータを設定します。
     * 残像、軌跡、バーストの3種類のエミッターを初期化します。
     */
    void Initialize();

    /**
     * @brief 回避エフェクトの再生開始
     * 
     * 回避動作の開始時に呼び出されます。
     * バーストエフェクトと移動軌跡エフェクトを開始します。
     * 
     * @param position エフェクト発生位置（プレイヤーの位置）
     * @param direction プレイヤーの移動方向ベクトル
     */
    void PlayEffect(const Vector3& position, const Vector3& direction);

    /**
     * @brief 残像エフェクトの生成
     * 
     * プレイヤーの現在位置に残像パーティクルを生成します。
     * 高速移動中に連続して呼び出すことで、残像が残る効果を実現します。
     * 
     * @param position 残像を生成する位置
     * @param rotation 残像の回転（プレイヤーの向き）
     */
    void CreateAfterImage(const Vector3& position, const Vector3& rotation);

    /**
     * @brief 回避終了時のフェードアウトエフェクト
     * 
     * 回避動作の終了時に呼び出され、軌跡エフェクトを停止します。
     * 
     * @param position エフェクト停止位置
     */
    void PlayFadeOutEffect(const Vector3& position);

private:
    std::unique_ptr<ParticleEmitter> afterImageEmitter_;  ///< 残像用パーティクルエミッタ
    std::unique_ptr<ParticleEmitter> trailEmitter_;       ///< 高速移動時の軌跡エミッタ
    std::unique_ptr<ParticleEmitter> burstEmitter_;       ///< 回避開始時のバーストエミッタ
    std::unique_ptr<ParticleEmitter> finishEmitter_;      ///< 回避完了時のエミッタ

    const std::string trailTexturePath_ = "./Resources/circle2.png";      ///< 軌跡エフェクト用テクスチャ
    const std::string burstTexturePath_ = "./Resources/circle2.png";      ///< バーストエフェクト用テクスチャ
    const std::string afterImageTexturePath_ = "./Resources/circle2.png"; ///< 残像エフェクト用テクスチャ

    Vector4 trailColor_ = { 0.2f, 0.5f, 1.0f, 0.7f };    ///< 軌跡の色（青白い色）
    Vector4 burstColor_ = { 0.1f, 0.4f, 1.0f, 0.8f };    ///< バーストの色（青色）
    Vector4 afterImageColor_ = { 0.8f, 0.9f, 1.0f, 0.6f }; ///< 残像の色（薄い青白色）
};