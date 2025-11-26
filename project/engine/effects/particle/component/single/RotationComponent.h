#pragma once
#include "base/GraphicsTypes.h"
#include "effects/particle/component/interface/IParticleBehaviorComponent.h"

/**
 * @brief パーティクルに回転速度を適用するコンポーネント
 * 
 * 毎フレーム指定された回転速度をパーティクルの回転角度に加算する。
 * 各軸（X, Y, Z）ごとに異なる回転速度を設定可能。
 */
class RotationComponent : public IParticleBehaviorComponent
{
public:
    /**
     * @brief コンストラクタ
     * @param rotSpeed 回転速度ベクトル（各軸のラジアン/フレーム）
     */
    explicit RotationComponent(const Vector3& rotSpeed);

    /**
     * @brief パーティクルの回転を更新する
     * @param particle 更新対象のパーティクル
     */
    void Update(Particle& particle) override;

private:
    // 回転速度ベクトル（各軸のラジアン/フレーム）
    Vector3 rotationSpeed_;
};
