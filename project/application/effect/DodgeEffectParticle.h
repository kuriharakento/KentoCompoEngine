#pragma once
// effects
#include "effects/particle/ParticleEmitter.h"
// math
#include "math/Vector3.h"

// 回避時のエフェクトを管理するクラス
class DodgeEffectParticle
{
public:
    DodgeEffectParticle();
    ~DodgeEffectParticle();

    // 初期化処理
    void Initialize();

    // 回避エフェクト開始（位置とプレイヤーの向きを指定）
    void PlayEffect(const Vector3& position, const Vector3& direction);

    // 残像エフェクトを生成（キャラクターの現在位置で残像を表示）
    void CreateAfterImage(const Vector3& position, const Vector3& rotation);

    // 消滅時のエフェクト（回避終了時のエフェクト）
    void PlayFadeOutEffect(const Vector3& position);

private:
    // 残像用パーティクルエミッタ
    std::unique_ptr<ParticleEmitter> afterImageEmitter_;

    // 高速移動時の軌跡エミッタ
    std::unique_ptr<ParticleEmitter> trailEmitter_;

    // 回避開始時のバーストエミッタ
    std::unique_ptr<ParticleEmitter> burstEmitter_;

    // 回避完了時のエミッタ
    std::unique_ptr<ParticleEmitter> finishEmitter_;

    // エフェクト用テクスチャパス
    const std::string trailTexturePath_ = "./Resources/circle2.png";
    const std::string burstTexturePath_ = "./Resources/circle2.png";
    const std::string afterImageTexturePath_ = "./Resources/circle2.png";

    // 色設定
    Vector4 trailColor_ = { 0.2f, 0.5f, 1.0f, 0.7f };    // 青白い色
    Vector4 burstColor_ = { 0.1f, 0.4f, 1.0f, 0.8f };    // 青色
    Vector4 afterImageColor_ = { 0.8f, 0.9f, 1.0f, 0.6f }; // 薄い青白色
};