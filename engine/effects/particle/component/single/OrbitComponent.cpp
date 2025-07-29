#include "OrbitComponent.h"

OrbitComponent::OrbitComponent(const Vector3& c, float radius_, float speed)
    : center_(c), radius_(radius_), angularSpeed_(speed)
{
}

OrbitComponent::OrbitComponent(const Vector3* target, float radius_, float speed) : target_(target), radius_(radius_), angularSpeed_(speed)
{
	if (target_)
	{
		center_ = *target_;
	}
	else
	{
        center_ = Vector3();
	}
}

void OrbitComponent::Update(Particle& particle)
{
	if (target_)
	{
		center_ = *target_;
	}

    float angle = angularSpeed_;

    Vector3 offset = particle.transform.translate - center_;

    float cosA = std::cos(angle);
    float sinA = std::sin(angle);

    float x = offset.x * cosA - offset.z * sinA;
    float z = offset.x * sinA + offset.z * cosA;

    offset.x = x;
    offset.z = z;

    particle.transform.translate = center_ + offset;
}
