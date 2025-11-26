#pragma once
#include <cmath>
#include "effects/particle/component/interface/IParticleBehaviorComponent.h"

/**
 * @brief パーティクルを中心点の周りで円軌道運動させるコンポーネント
 * 
 * Y軸を中心とした回転行列を使用し、パーティクルを指定された中心点の周りで
 * 円運動させる。静的な中心座標またはポインタによる動的な中心をサポートする。
 */
class OrbitComponent : public IParticleBehaviorComponent
{
public:
    /**
     * @brief コンストラクタ（静的中心）
     * @param c 軌道の中心座標
     * @param radius_ 軌道半径（現在は初期オフセットで決まるため未使用）
     * @param speed 角速度（ラジアン/フレーム）
     */
    OrbitComponent(const Vector3& c, float radius_, float speed);

    /**
     * @brief コンストラクタ（動的中心）
     * @param target 軌道の中心座標へのポインタ（毎フレーム参照）
     * @param radius_ 軌道半径（現在は初期オフセットで決まるため未使用）
     * @param speed 角速度（ラジアン/フレーム）
     */
	OrbitComponent(const Vector3* target, float radius_, float speed);

    /**
     * @brief パーティクルを軌道に沿って移動させる
     * @param particle 更新対象のパーティクル
     */
    void Update(Particle& particle) override;

private:
    // 動的中心座標へのポインタ（nullptr時は静的中心を使用）
	const Vector3* target_ = nullptr;
    // 軌道の中心座標
    Vector3 center_;
    // 角速度（ラジアン/フレーム）
    float angularSpeed_;
    // 軌道半径
    float radius_;
};
