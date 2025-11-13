#pragma once
#include <memory>
#include <vector>
#include "effects/particle/ParticleEmitter.h"
#include "math/Vector3.h"

/**
 * @brief タイトル画面用炎柱エフェクトクラス
 * 
 * タイトル画面で画面奥に向かって連続的に炎柱が立ち上がる演出を管理します。
 * カメラの進行に合わせて左右に炎柱を定期的に発生させ、ダイナミックな背景演出を実現します。
 * 
 * 主な機能:
 * - 左右2レーンの炎柱エフェクト
 * - カメラ位置追従による床面エフェクト
 * - 定期的な発火タイミング制御
 * - 上昇する炎のパーティクル表現
 * 
 * @code
 * // 使用例
 * TitleFireEffect fireEffect;
 * fireEffect.Initialize();
 * 
 * // 毎フレーム
 * fireEffect.Update(cameraPosition);
 * @endcode
 */
class TitleFireEffect
{
public:
    /**
     * @brief エフェクトシステムの初期化
     * 
     * 左右の炎柱エミッターと床面エフェクトエミッターを初期化します。
     */
    void Initialize();
    
    /**
     * @brief エフェクトの更新
     * 
     * カメラ位置に応じて床面エフェクトを追従させ、一定間隔で炎柱を発生させます。
     * 
     * @param cameraPos カメラの現在位置
     */
    void Update(const Vector3& cameraPos);
    
    /**
     * @brief 炎柱の発生
     * 
     * 指定位置の前方に左右2本の炎柱を発生させます。
     * 
     * @param position 発生基準位置
     */
    void EmitFire(const Vector3& position);

private:
    std::unique_ptr<ParticleEmitter> fireEmitterRight_;  ///< 右側の炎柱エミッター
    std::unique_ptr<ParticleEmitter> fireEmitterLeft_;   ///< 左側の炎柱エミッター
    std::unique_ptr<ParticleEmitter> floorEmitter_;      ///< 床面エフェクトエミッター
    Vector3 floorPos = {};                                ///< 床面エフェクトの位置
    float lastFireZ_ = 0.0f;                              ///< 最後に炎を発生させたZ座標
    const float interval_ = 1.0f;                         ///< 炎発生の時間間隔（秒）
    const float laneOffset_ = 1.5f;                       ///< 左右のレーンオフセット（中心からの距離）
    const float groundY_ = -0.5f;                         ///< 地面のY座標
	float time = 0.0f;                                    ///< タイマー（秒）
	const std::string fireTexturePath_ = "./Resources/gradation.png";  ///< 炎エフェクト用テクスチャ
	bool firstUpdate_ = true;                             ///< 初回更新フラグ
};