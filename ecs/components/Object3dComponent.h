#pragma once
#include <memory>
#include "../../../engine/graphics/3d/Object3d.h"

namespace KCE
{
namespace ecs
{
    /**
     * @brief 単体描画用のコンポーネント。内部に Object3d インスタンスを保持する。
     */
    struct Object3dComponent
    {
        // 描画の実体。所有する
        std::unique_ptr<Object3d> object3d_ = nullptr;
        // 描画を有効にするかどうかのフラグ
        bool isVisible_ = true;
    };
}
} // namespace KCE
