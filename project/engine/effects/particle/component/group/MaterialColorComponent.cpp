#include "MaterialColorComponent.h"

MaterialColorComponent::MaterialColorComponent(const Vector4& color)
    : color_(color)
{
}

void MaterialColorComponent::Update(ParticleGroup& group)
{
    // パーティクルグループのマテリアルカラーを設定
    group.SetMaterialColor(color_);
}
