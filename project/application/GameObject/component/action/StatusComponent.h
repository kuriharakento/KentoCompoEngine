#pragma once
#include "application/GameObject/Combatable/base/StatusSystem.h"
#include "application/GameObject/component/base/IGameObjectComponent.h"

class StatusComponent : public IGameObjectComponent
{
public:
	// ステータスの更新
	void Update(GameObject* owner) override;

	StatusValue hp{ 100.0f };
    StatusValue maxHp{ 100.0f };
    StatusValue attackPower{ 10.0f };
	bool isAlive = true;
};