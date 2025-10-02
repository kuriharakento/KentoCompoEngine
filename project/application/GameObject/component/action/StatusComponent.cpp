#include "StatusComponent.h"

#include "time/TimeManager.h"

void StatusComponent::Update(GameObject* owner)
{
    float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;
    hp.Update(deltaTime);
    maxHp.Update(deltaTime);
    attackPower.Update(deltaTime);
	if (hp.GetValue() <= 0.0f)
	{
		isAlive = false;
	}
}
