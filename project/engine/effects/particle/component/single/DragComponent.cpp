#include "DragComponent.h"

#include "base/GraphicsTypes.h"

DragComponent::DragComponent(float drag)
    : dragFactor_(drag)
{
}

void DragComponent::Update(Particle& particle)
{
    // 速度にドラッグ係数を乗算（v = v * drag）
    // drag < 1.0 で減速、drag > 1.0 で加速
    particle.velocity *= dragFactor_;
}
