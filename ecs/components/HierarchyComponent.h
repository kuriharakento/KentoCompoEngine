#pragma once

#include "../../../engine/ecs/Entity.h"

namespace KCE
{
namespace ecs
{
    /**
     * @brief エンティティ間の親子関係を表現するコンポーネント。
     */
    struct HierarchyComponent
    {
        // 親エンティティ
        EntityID parent_ = kInvalidEntity;

        // 最初の子エンティティ
        EntityID firstChild_ = kInvalidEntity;

        // 次の兄弟エンティティ
        EntityID nextSibling_ = kInvalidEntity;
    };
}
} // namespace KCE
