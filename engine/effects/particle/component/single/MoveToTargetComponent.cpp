#include "MoveToTargetComponent.h"

#include "math/Easing.h"
#include "math/MathUtils.h"

MoveToTargetComponent::MoveToTargetComponent(const Vector3& target, const float& speed) : target_(target), speed_(speed)
{
}

MoveToTargetComponent::MoveToTargetComponent(const Vector3* target, const float& speed) : targetPtr_(target), speed_(speed)
{
}

void MoveToTargetComponent::Update(Particle& particle)
{
	if (targetPtr_)
	{
		target_ = *targetPtr_;
	}
	// tは0〜1の補間係数（寿命に比例）
	float t = particle.currentTime / particle.lifeTime;
	t = MathUtils::Clamp(t, 0.0f, 1.0f);

	// 開始位置とターゲット間を線形補間
	particle.transform.translate.x = EasingToEnd<float>(particle.startPos.x, target_.x, EaseOutCirc<float>, t);
	particle.transform.translate.y = EasingToEnd<float>(particle.startPos.y, target_.y, EaseOutCirc<float>, t);
	particle.transform.translate.z = EasingToEnd<float>(particle.startPos.z, target_.z, EaseOutCirc<float>, t);
}