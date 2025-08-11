#include "UVTranslateComponent.h"

#include "time/TimeManager.h"

UVTranslateComponent::UVTranslateComponent(const Vector3& translate)
    : translate_(translate)
{
}

void UVTranslateComponent::Update(ParticleGroup& group)
{
	float delta = TimeManager::GetInstance().GetDeltaTime();
    Vector3 currentTranslate = group.GetUVTranslate();
    currentTranslate += translate_ * delta;
	if (currentTranslate.x > 1.0f) currentTranslate.x -= 1.0f;
	if (currentTranslate.x < 0.0f) currentTranslate.x += 1.0f;

	if (currentTranslate.y > 1.0f) currentTranslate.y -= 1.0f;
	if (currentTranslate.y < 0.0f) currentTranslate.y += 1.0f;

    group.SetUVTranslate(currentTranslate);
}
