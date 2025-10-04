#pragma once
#include <memory>
#include "effects/particle/ParticleEmitter.h"
#include "math/Vector3.h"

// カーネージモード中のダーク＆派手演出クラス
class CarnageModeEffect
{
public:
    CarnageModeEffect();
    ~CarnageModeEffect();

    // 初期化
	void Initialize();

    // オーラ（炎＋黒煙＋回転）
    void PlayAuraEffect(const Vector3& position);

    // 移動軌跡（赤黒炎＋稲妻・残像）
    void PlayTrailEffect(const Vector3& position, const Vector3& direction);

    // 終了バースト（爆発的な炎＋闇の煙・稲妻）
    void PlayEndEffect(const Vector3& position);

    // 毎フレーム更新
    void Update(float deltaTime);

private:
    std::unique_ptr<ParticleEmitter> auraEmitter_;   // 炎オーラ
    std::unique_ptr<ParticleEmitter> smokeEmitter_;  // 黒煙
    std::unique_ptr<ParticleEmitter> trailEmitter_;  // 軌跡炎
    std::unique_ptr<ParticleEmitter> lightningEmitter_; // 稲妻
    std::unique_ptr<ParticleEmitter> burstEmitter_;  // 終了バースト

    // テクスチャ
    const std::string auraTexturePath_ = "./Resources/circle2.png";
    const std::string smokeTexturePath_ = "./Resources/circle2.png";
    const std::string trailTexturePath_ = "./Resources/circle2.png";
    const std::string lightningTexturePath_ = "./Resources/circle2.png";
    const std::string burstTexturePath_ = "./Resources/circle2.png";
};