#pragma once
#include <array>
#include <cstdint>

class ParticleEffect;

/**
 * @brief プレイヤースキルの解放状況とクールタイムを管理するコンポーネント。
 */
struct SkillComponent
{
    // 各スキルの解放状況
    bool isLmbUnlocked_ = true; 
    bool isRmbUnlocked_ = false;
    bool isDecoyUnlocked_ = false;
    bool isImpactUnlocked_ = false;
    bool isBeamUnlocked_ = false;

    // クールタイムタイマー (0以下で発動可能)
    float lmbTimer_ = 0.0f;
    float rmbTimer_ = 0.0f;
    float decoyTimer_ = 0.0f;
    float impactTimer_ = 0.0f;
    float beamTimer_ = 0.0f;

    // 現在再生中のビームパーティクル (1つのみ)
    ParticleEffect* activeBeamParticle_ = nullptr;
    float beamActiveTimer_ = 0.0f;

    // 定数（初期値）: 実際は武器やスキルによって変動
    static constexpr float kLmbCooldown = 0.9f;
    static constexpr float kRmbCooldown = 0.4f;
    static constexpr float kDecoyCooldown = 5.0f;
    static constexpr float kImpactCooldown = 2.0f;
    static constexpr float kBeamCooldown = 20.0f;
};
