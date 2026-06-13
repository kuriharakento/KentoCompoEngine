#pragma once


#include "ISystem.h"
#include "../../../engine/ecs/Registry.h"

class Camera;

class Object3dSystem : public ISystem
{
public:
    /**
     * @brief 登録された単体描画用のObject3dをすべて描画する、E
     * @param registry ECSレジストリ
     * @param camera 使用するカメラ
     */
    void Draw(Registry& registry, Camera* camera);
    void DrawGBuffer(Registry& registry, Camera* camera);
    void DrawShadow(Registry& registry, Camera* camera);
};
