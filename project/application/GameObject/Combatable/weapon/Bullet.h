#pragma once

#include "math/Vector3.h"
#include <memory>

#include "application/GameObject/base/GameObject.h"
#include "application/GameObject/Combatable/base/CombatableObject.h"

class Bullet : public CombatableObject
{
public:
	~Bullet() = default;
	Bullet(std::string tag) : CombatableObject(tag){}

	void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, const Vector3& position);
	void Update(float deltaTime);
	void Draw(CameraManager* camera);
};
