#include "MaterialColorComponent.h"

MaterialColorComponent::MaterialColorComponent(const Vector4& color)
    : color_(color)
{
}

void MaterialColorComponent::Update(ParticleGroup& group)
{
    group.SetMaterialColor(color_);
}
