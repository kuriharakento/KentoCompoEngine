#pragma once

#include "../../../engine/ecs/Registry.h"

class Camera;

class Object3dSystem
{
public:
    /**
     * @brief 登録された単体描画用のObject3dをすべて描画する。
     * @param registry ECSレジストリ
     * @param camera 使用するカメラ
     */
    static void Draw(Registry& registry, Camera* camera);
};
