#pragma once
#include "application/GameObject/base/GameObject.h"
#include "application/GameObject/component/base/ICollisionComponent.h"
#include "math/OBB.h"

class Obstacle : public GameObject
{
public:
	virtual ~Obstacle() = default;
	explicit Obstacle(const std::string& tag) : GameObject(tag) {}
	virtual void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager);
	virtual void Update();
	virtual void Draw(CameraManager* camera);
	void AddComponent(const std::string& name, std::unique_ptr<IGameObjectComponent> comp);

protected:
	void CollisionSettings(ICollisionComponent* collider);
	void ResolvePenetration(GameObject* other);
	bool CheckOBBvsOBBMTV(const OBB& obbA, const OBB& obbB, Vector3& mtv) const;
};

