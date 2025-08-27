#pragma once
#include "Obstacle.h"

class BarrierBlock : public Obstacle
{
public:
	explicit BarrierBlock(const std::string& tag = GameObjectTag::Item::BarrierBlock) : Obstacle(tag) {}
	void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager) override;
	void Update() override;
	void Draw(CameraManager* camera) override;
	void CollisionSettings(ICollisionComponent* collider) override;
};

