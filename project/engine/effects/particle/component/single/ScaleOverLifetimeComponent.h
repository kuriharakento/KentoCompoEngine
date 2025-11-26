#pragma once
#include "effects/particle/component/interface/IParticleBehaviorComponent.h"

/**
 * @brief パーティクルの生存時間に応じてスケールを変化させるコンポーネント
 * 
 * パーティクルの生存時間の割合（0.0〜1.0）に基づいて、
 * 開始スケールから終了スケールへ線形補間でスケールを変化させます。
 */
class ScaleOverLifetimeComponent : public IParticleBehaviorComponent
{
public:
    /**
     * @brief コンストラクタ
     * @param start 開始時のスケール値
     * @param end 終了時のスケール値
     */
    ScaleOverLifetimeComponent(float start, float end);

    /**
     * @brief パーティクルのスケールを更新する
     * @param particle 更新対象のパーティクル
     */
    void Update(Particle& particle) override;

private:
    float startScale_; // 開始時のスケール
    float endScale_;   // 終了時のスケール
};
