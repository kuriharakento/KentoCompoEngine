#pragma once
#include "effects/particle/ParticleGroup.h"
#include "effects/particle/component/interface/IParticleGroupComponent.h"

/**
 * @brief UV回転コンポーネント
 * 
 * パーティクルグループ全体のUV座標を回転させるコンポーネント。
 * IParticleGroupComponentを継承し、テクスチャの回転アニメーションを実現する。
 */
class UVRotateComponent : public IParticleGroupComponent
{
public:
    /**
     * @brief コンストラクタ
     * @param rotate 1フレームあたりのUV回転量
     */
    explicit UVRotateComponent(const Vector3& rotate);

    /**
     * @brief パーティクルグループを更新する
     * @param group 更新対象のパーティクルグループ
     */
    void Update(ParticleGroup& group) override;

private:
    // 1フレームあたりのUV回転量
    Vector3 rotate_;
};
