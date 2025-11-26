#pragma once
#include "effects/particle/component/interface/IParticleBehaviorComponent.h"

/**
 * @brief パーティクルに一定の加速度を与えるコンポーネント
 * 
 * 毎フレーム指定された加速度ベクトルをパーティクルの速度に加算する。
 * 物理シミュレーションにおける加速度の概念を実現し、
 * パーティクルの動きに変化を与える。
 */
class AccelerationComponent : public IParticleBehaviorComponent
{
public:
    /**
     * @brief コンストラクタ
     * @param accel 加速度ベクトル（毎フレーム速度に加算される値）
     */
    explicit AccelerationComponent(const Vector3& accel);

    /**
     * @brief パーティクルの速度を更新する
     * @param particle 更新対象のパーティクル
     */
    void Update(Particle& particle) override;

private:
    // 加速度ベクトル
    Vector3 acceleration_;
};
