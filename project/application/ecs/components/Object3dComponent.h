#pragma once
#include <memory>
#include "../../../engine/graphics/3d/Object3d.h"

// Deleted namespace

/**
 * @brief 単体描画用のコンポーネント。
 *        内部に Object3d インスタンスを保持する。
 */
struct Object3dComponent
{
    std::unique_ptr<Object3d> object3d_ = nullptr;
    bool isVisible_ = true;
};
