#pragma once
#include "effects/particle/component/interface/IParticleBehaviorComponent.h"

/**
 * @brief パーティクルに空気抵抗（ドラッグ）を与えるコンポーネント
 * 
 * 毎フレーム指定された係数をパーティクルの速度に乗算する。
 * 物理シミュレーションにおける空気抵抗や摩擦の影響を再現し、
 * パーティクルの動きを徐々に減速させる。
 */
class DragComponent : public IParticleBehaviorComponent
{
public:
    /**
     * @brief コンストラクタ
     * @param drag ドラッグ係数（0.0〜1.0、1.0で抵抗なし、0.0で即座に停止）
     */
    explicit DragComponent(float drag);

    /**
     * @brief パーティクルの速度にドラッグを適用する
     * @param particle 更新対象のパーティクル
     */
    void Update(Particle& particle) override;

private:
    // ドラッグ係数（速度に乗算される値）
    float dragFactor_;
};
