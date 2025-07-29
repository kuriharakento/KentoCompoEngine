#pragma once
#include "application/GameObject/base/GameObject.h"

class IActionComponent : public virtual IGameObjectComponent
{
public:
	virtual ~IActionComponent() = default;
	virtual void Draw(CameraManager* camera) {}
};
