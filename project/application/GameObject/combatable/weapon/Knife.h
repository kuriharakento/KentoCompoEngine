#pragma once
#include "application/GameObject/combatable/base/CombatableObject.h"

class Knife : public CombatableObject
{
public:
	~Knife() = default;
	Knife(std::string tag) : CombatableObject(tag) {}

	void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager);
	void Update();
	void Draw(CameraManager* camera);
};

