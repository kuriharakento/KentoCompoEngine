#pragma once
#include "effects/particle/ParticleGroup.h"
#include "effects/particle/component/interface/IParticleGroupComponent.h"
#include "math/Vector3.h"

/**
 * @brief UV平行移動コンポーネント
 * 
 * パーティクルグループ全体のUV座標を平行移動させるコンポーネント。
 * IParticleGroupComponentを継承し、テクスチャのスクロールアニメーションを実現する。
 * UV座標は0.0〜1.0の範囲でラップされる。
 */
class UVTranslateComponent : public IParticleGroupComponent
{
public:
    /**
     * @brief コンストラクタ
     * @param translate 1秒あたりのUV平行移動量
     */
    explicit UVTranslateComponent(const Vector3& translate);

    /**
     * @brief パーティクルグループを更新する
     * @param group 更新対象のパーティクルグループ
     */
    void Update(ParticleGroup& group) override;

private:
    // 1秒あたりのUV平行移動量
    Vector3 translate_;
};
