#pragma once
#include "Component.h"

namespace KCE
{
namespace GameObjectComponent
{
/** @brief ゲームの振る舞いと任意の描画を持つコンポーネント。 */
class Behaviour : public Component
{
public:
    ~Behaviour() override = default;
    /** @brief 3D 描画を行う。 */
    virtual void Draw3D(CameraManager*) {}
    /** @brief 2D 描画を行う。 */
    virtual void Draw2D() {}
};
}
} // namespace KCE