#pragma once
#include "application/GameObject/base/GameObject.h"
#include "math/OBB.h"
#include "math/Vector3.h"

class GameObject;

struct CollisionInfo
{
    bool    isColliding = false;
    Vector3 mtvAxis{ 0, 1, 0 };
    float   mtvDepth = FLT_MAX;
};

namespace CollisionUtils
{
	bool CheckOBBvsOBBMTV(const OBB& obbA, const OBB& obbB, Vector3& mtv);
    void ResolvePenetration(GameObject* self, GameObject* other);
}
