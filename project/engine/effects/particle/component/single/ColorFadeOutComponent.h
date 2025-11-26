#pragma once
#include "effects/particle/component/interface/IParticleBehaviorComponent.h"

/**
 * @brief パーティクルの色をフェードアウトさせるコンポーネント
 * 
 * パーティクルの寿命に基づいてアルファ値（透明度）を減少させ、
 * 寿命が尽きる頃には完全に透明になるようにする。
 */
class ColorFadeOutComponent : public IParticleBehaviorComponent
{
public:
    /**
     * @brief パーティクルの透明度を更新する
     * @param particle 更新対象のパーティクル
     */
    void Update(Particle& particle) override;
};
