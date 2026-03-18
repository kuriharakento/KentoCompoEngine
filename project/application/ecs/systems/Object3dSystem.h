#pragma once

#include "../../../engine/ecs/Registry.h"

class Camera;

class Object3dSystem
{
public:
    static void Draw(Registry& registry, Camera* camera);
};
