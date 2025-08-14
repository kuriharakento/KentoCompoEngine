#pragma once
#include "application/GameObject/Combatable/character/base/Character.h"
#include "application/GameObject/component/base/ICollisionComponent.h"

class Player : public Character
{
public:
	~Player() = default;
	Player(std::string tag) : Character(tag) {}
	void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager) override;
	void Update() override;
	void Draw(CameraManager* camera) override;

private:
	void CollisionSettings(ICollisionComponent* collider) override;
};
