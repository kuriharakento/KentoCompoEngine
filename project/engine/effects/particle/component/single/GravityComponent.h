#pragma once
#include "effects/particle/component/interface/IParticleBehaviorComponent.h"

/**
 * @brief パーティクルに重力を与えるコンポーネント
 * 
 * 毎フレーム指定された重力ベクトルをパーティクルの速度に加算する。
 * 物理シミュレーションにおける重力の影響を再現し、
 * パーティクルを自然に落下させたり、特定方向への力を与えたりする。
 */
class GravityComponent : public IParticleBehaviorComponent
{
public:
    /**
     * @brief コンストラクタ
     * @param g 重力ベクトル（一般的には下向き、例: (0, -9.8f, 0)）
     */
    explicit GravityComponent(const Vector3& g);

    /**
     * @brief パーティクルの速度に重力を適用する
     * @param particle 更新対象のパーティクル
     */
    void Update(Particle& particle) override;

private:
    // 重力ベクトル
	Vector3 gravity;
};
