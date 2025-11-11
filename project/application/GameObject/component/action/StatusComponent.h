#pragma once
#include "application/GameObject/Combatable/base/StatusSystem.h"
#include "application/GameObject/component/base/IActionComponent.h"

class StatusComponent : public IActionComponent
{
public:
	// ステータスの更新
	void Update(GameObject* owner) override;
	void Draw(CameraManager* camera) override {}

	StatusValue hp{ 100.0f };
    StatusValue maxHp{ 100.0f };
    StatusValue attackPower{ 10.0f };
	StatusValue moveSpeed{ 9.0f };
	bool isAlive = true;
};