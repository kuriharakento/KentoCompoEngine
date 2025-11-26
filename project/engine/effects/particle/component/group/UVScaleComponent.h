#pragma once
#include "effects/particle/ParticleGroup.h"
#include "effects/particle/component/interface/IParticleGroupComponent.h"
#include "math/Vector3.h"

/**
 * @brief UVスケールコンポーネント
 * 
 * パーティクルグループ全体のUV座標をスケーリングするコンポーネント。
 * IParticleGroupComponentを継承し、テクスチャのスケールアニメーションを実現する。
 */
class UVScaleComponent : public IParticleGroupComponent
{
public:
    /**
     * @brief コンストラクタ
     * @param scale 1フレームあたりのUVスケール変化量
     */
    explicit UVScaleComponent(const Vector3& scale);

    /**
     * @brief パーティクルグループを更新する
     * @param group 更新対象のパーティクルグループ
     */
    void Update(ParticleGroup& group) override;

private:
    // 1フレームあたりのUVスケール変化量
    Vector3 scale_;
};
