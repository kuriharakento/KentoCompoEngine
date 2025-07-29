#include "BounceComponent.h"

BounceComponent::BounceComponent(float groundHeight, float restitution, float minVelocity) : groundHeight_(groundHeight), restitution_(restitution), minVelocity_(minVelocity)
{
}

void BounceComponent::Update(Particle& particle)
{
	// 次のフレームでのY座標を予測
	float nextY = particle.transform.translate.y + particle.velocity.y * (1.0f / 60.0f);

	// 地面に到達
	if (particle.transform.translate.y >= groundHeight_ && nextY < groundHeight_)
	{
		particle.transform.translate.y = groundHeight_ + particle.transform.scale.y * 0.5f;
		particle.velocity.y = -particle.velocity.y * restitution_;
	}
}

