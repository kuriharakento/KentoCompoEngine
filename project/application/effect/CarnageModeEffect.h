#pragma once
#include <memory>
#include "effects/particle/ParticleEmitter.h"
#include "math/Vector3.h"

/**
 * @brief カーネージモードエフェクトクラス
 * 
 * プレイヤーの特殊な強化状態「カーネージモード」中に発生する、
 * ダークで派手な視覚演出を管理します。赤黒い炎、黒煙、稲妻などの
 * 複数のパーティクルエフェクトを組み合わせて、圧倒的な破壊力を表現します。
 * 
 * 主な機能:
 * - 周囲を包む赤黒いオーラエフェクト
 * - 移動時の炎と稲妻の軌跡
 * - 終了時の爆発的バーストエフェクト
 * - 大型で派手なパーティクル設定
 * 
 * @code
 * // 使用例
 * CarnageModeEffect carnageEffect;
 * carnageEffect.Initialize();
 * 
 * // モード開始時
 * carnageEffect.PlayAuraEffect(playerPos);
 * 
 * // 移動中
 * carnageEffect.PlayTrailEffect(playerPos, moveDirection);
 * 
 * // モード終了時
 * carnageEffect.PlayEndEffect(playerPos);
 * @endcode
 */
class CarnageModeEffect
{
public:
    CarnageModeEffect();
    ~CarnageModeEffect();

    /**
     * @brief エフェクトシステムの初期化
     * 
     * 5種類のパーティクルエミッター（オーラ、煙、軌跡、稲妻、バースト）を初期化します。
     * 各エミッターには複数のコンポーネントが設定され、複雑な視覚効果を実現します。
     */
	void Initialize();

    /**
     * @brief オーラエフェクトの再生
     * 
     * プレイヤーの周囲を包む赤黒い炎と黒煙のオーラを発生させます。
     * カーネージモード開始時や持続中に使用します。
     * 
     * @param position エフェクト発生位置（プレイヤーの位置）
     */
    void PlayAuraEffect(const Vector3& position);

    /**
     * @brief 移動軌跡エフェクトの再生
     * 
     * 高速移動時に赤黒い炎の軌跡と稲妻の閃光を発生させます。
     * 移動の方向性を視覚的に強調します。
     * 
     * @param position エフェクト発生位置
     * @param direction 移動方向ベクトル
     */
    void PlayTrailEffect(const Vector3& position, const Vector3& direction);

    /**
     * @brief 終了バーストエフェクトの再生
     * 
     * カーネージモード終了時に爆発的な炎、煙、稲妻を発生させます。
     * 大規模な視覚演出により、モードの終わりを印象的に表現します。
     * 
     * @param position エフェクト発生位置
     */
    void PlayEndEffect(const Vector3& position);

    /**
     * @brief エフェクトの更新処理
     * 
     * 毎フレーム呼び出されます。現在は未使用ですが、
     * 将来的にエミッターの更新処理を追加する予定です。
     * 
     * @param deltaTime フレーム間の経過時間（秒）
     */
    void Update(float deltaTime);

private:
    std::unique_ptr<ParticleEmitter> auraEmitter_;      ///< 赤黒炎オーラエミッター（超大型）
    std::unique_ptr<ParticleEmitter> smokeEmitter_;     ///< 黒煙エミッター（ダーク感の強調）
    std::unique_ptr<ParticleEmitter> trailEmitter_;     ///< 移動軌跡の炎エミッター
    std::unique_ptr<ParticleEmitter> lightningEmitter_; ///< 稲妻の閃光エミッター
    std::unique_ptr<ParticleEmitter> burstEmitter_;     ///< 終了時の爆発バーストエミッター

    // エフェクト用テクスチャパス
    const std::string auraTexturePath_ = "./Resources/circle2.png";
    const std::string smokeTexturePath_ = "./Resources/circle2.png";
    const std::string trailTexturePath_ = "./Resources/circle2.png";
    const std::string lightningTexturePath_ = "./Resources/circle2.png";
    const std::string burstTexturePath_ = "./Resources/circle2.png";
};