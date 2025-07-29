#pragma once

#include "application/GameObject/component/base/ICollisionComponent.h"
#include "math/AABB.h"
#include "Math/Vector3.h"

class AABBColliderComponent : public ICollisionComponent
{
public:
	AABBColliderComponent(GameObject* owner);
	~AABBColliderComponent();

	void Update(GameObject* owner) override;
	ColliderType GetColliderType() const override { return ColliderType::AABB; }
	void SetAABB(const AABB& aabb) { aabb_ = aabb; }
	const AABB& GetAABB() const { return aabb_; }

private:
	AABB aabb_;				// AABB
};
