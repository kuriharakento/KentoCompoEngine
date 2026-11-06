#include "MotionComponents.h"
#include "engine/gameobject/base/GameObject.h"
#include "engine/gameobject/component/base/ComponentFactory.h"
#include <cmath>
#include <numbers>

// ファクトリに登録
REGISTER_COMPONENT(Rotator)
REGISTER_COMPONENT(Bobbing)

namespace KCE::GameObjectComponent
{
	namespace
	{
		constexpr float kDegreesToRadians = std::numbers::pi_v<float> / 180.0f;
		constexpr float kTwoPi = std::numbers::pi_v<float> * 2.0f;
	}

	void ForceLinkMotionComponents() {}

	Rotator::Rotator()
	{
		Register("degPerSec", &degPerSec_);
	}

	void Rotator::Update()
	{
		// 持ち主の時計で進める（止めたり倍速にしたりできるように）
		const float step = GetDeltaTime() * kDegreesToRadians;
		if (step == 0.0f)
		{
			return;
		}
		GameObject* owner = GetOwner();
		owner->SetRotation(owner->GetRotation() + degPerSec_ * step);
	}

	Bobbing::Bobbing()
	{
		Register("amplitude", &amplitude_);
		Register("frequency", &frequency_);
		Register("phase", &phase_);
	}

	void Bobbing::Update()
	{
		const float deltaTime = GetDeltaTime();
		if (deltaTime == 0.0f)
		{
			return;
		}
		time_ += deltaTime;
		const Vector3 offset = amplitude_ * std::sin(kTwoPi * (frequency_ * time_ + phase_));
		GameObject* owner = GetOwner();
		owner->SetPosition(owner->GetPosition() + (offset - appliedOffset_));
		appliedOffset_ = offset;
	}

	void Bobbing::OnUpdateResumed()
	{
		time_ = 0.0f;
		appliedOffset_ = Vector3{};
	}
}
