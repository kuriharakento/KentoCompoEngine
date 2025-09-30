#include "ComboManager.h"

#include "time/TimeManager.h"

void ComboManager::OnEnemyDefeated(int count)
{
	comboCount_ += count;
	comboTimer_ = kComboTimeout;
}

void ComboManager::Update()
{
    if (comboCount_ > 0)
    {
		comboTimer_ -= TimeManager::GetInstance().GetGameContext().deltaTime;
        if (comboTimer_ <= 0.0f)
        {
            comboCount_ = 0;
            comboTimer_ = 0.0f;
        }
    }
}

void ComboManager::Reset()
{
    comboCount_ = 0;
    comboTimer_ = 0.0f;
}
