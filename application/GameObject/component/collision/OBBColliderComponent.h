#pragma once
#include "application/GameObject/component/base/ICollisionComponent.h"
#include "math/OBB.h"

class OBBColliderComponent : public ICollisionComponent
{
public:
	OBBColliderComponent(GameObject* owner);
	~OBBColliderComponent() override;

	void Update(GameObject* owner) override;
	ColliderType GetColliderType() const override { return ColliderType::OBB; }
	void SetOBB(const OBB& obb) { obb_ = obb; }
	const OBB& GetOBB() const { return obb_; }

private:
	OBB obb_;				// OBB
};

