#pragma once
#include "effects/particle/ParticleGroup.h"
#include "effects/particle/component/interface/IParticleGroupComponent.h"
#include "math/Vector4.h"

/**
 * @brief マテリアルカラー変更コンポーネント
 * 
 * パーティクルグループ全体のマテリアルカラーを設定するコンポーネント。
 * IParticleGroupComponentを継承し、グループ全体に同一のカラーを適用する。
 */
class MaterialColorComponent : public IParticleGroupComponent
{
public:
    /**
     * @brief コンストラクタ
     * @param color 設定するマテリアルカラー（RGBA）
     */
    explicit MaterialColorComponent(const Vector4& color);

    /**
     * @brief パーティクルグループを更新する
     * @param group 更新対象のパーティクルグループ
     */
    void Update(ParticleGroup& group) override;

private:
    // マテリアルカラー
    Vector4 color_;
};
