#pragma once
#include <memory>
#include <vector>
#include "effects/particle/ParticleEmitter.h"
#include "math/Vector3.h"

/**
 * @brief タイトル画面用炎エフェクトクラス
 * 
 * タイトル画面で画面奥に向かって連続的に炎が立ち上がる演出を管理します。
 * 左右のレーンに配置された炎と床面エフェクトを組み合わせた演出を行います。
 */
class TitleFireEffect
{
public:
    /**
     * @brief エフェクトシステムの初期化
     * 
     * 左右の炎エミッターと床面エフェクトエミッターを作成して登録します。
     */
    void Initialize();
    
    /**
     * @brief エフェクトの更新
     * 
     * カメラ位置に追従して床面エフェクトを配置し、
     * 一定間隔で新しい炎を発生させます。
     * 
     * @param cameraPos カメラの現在位置
     */
    void Update(const Vector3& cameraPos);
    
    /**
     * @brief 炎の発生
     * 
     * 指定位置の前方、左右のレーンに炎エフェクトを配置します。
     * 
     * @param position 発生基準位置
     */
    void EmitFire(const Vector3& position);

private:
    std::unique_ptr<ParticleEmitter> fireEmitterRight_;  ///< 右側の炎エミッター
    std::unique_ptr<ParticleEmitter> fireEmitterLeft_;   ///< 左側の炎エミッター
    std::unique_ptr<ParticleEmitter> floorEmitter_;      ///< 床面エフェクトエミッター
    Vector3 floorPos_ = {};                              ///< 床面エフェクトの位置
    float lastFireZ_ = 0.0f;                             ///< 最後に炎を発生させたZ座標
    const float interval_ = 1.0f;                        ///< 炎発生の時間間隔（秒）
    const float laneOffset_ = 1.5f;                      ///< 左右のレーンオフセット
    const float groundY_ = -0.5f;                        ///< 地面のY座標
float time_ = 0.0f;                                  ///< タイマー（秒）
const std::string fireTexturePath_ = "./Resources/gradation.png";  ///< 炎用テクスチャパス
bool firstUpdate_ = true;                            ///< 初回更新フラグ
};
