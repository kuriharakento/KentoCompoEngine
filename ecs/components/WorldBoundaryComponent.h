#pragma once
#include "math/Vector3.h"

namespace KCE
{
namespace ecs
{
    /**
     * @brief フィールドの移動制限を定義するコンポーネント。
     * 円形（XZ平面）の範囲内にエンティティを留める。
     */
    struct WorldBoundaryComponent
    {
        // 制限の半径
        float radius_ = 200.0f;
        // 有効フラグ
        bool active_ = true;
    };
}
} // namespace KCE
