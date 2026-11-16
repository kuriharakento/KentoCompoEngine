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
    /**
     * @brief 3D の半透明の描画を行う。Skybox のあと、半透明の物と同じ段階で呼ばれる
     * @details Draw3D（不透明のフォワード）で深度を書かずに描くと、あとから描かれる Skybox に上書きされる。
     *          深度を書かない物（ゲージなど）は、こちらで描く
     */
    virtual void DrawTransparent3D(CameraManager*) {}
    /** @brief 2D 描画を行う。 */
    virtual void Draw2D() {}
};
}
} // namespace KCE