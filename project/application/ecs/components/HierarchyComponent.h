#pragma once

#include "../../../engine/ecs/Entity.h"

// No namespaces

/**
 * @brief エンティティ間の親子関係を表現する。
 * 
 * ポインタを使わずEntityIDのリンクリスト構造にすることで、メモリ連続性を保つ。
 */
struct HierarchyComponent
{
    // 親。ルートなら kInvalidEntity
    EntityID parent = kInvalidEntity;

    // 最初の子。いないなら kInvalidEntity
    EntityID firstChild = kInvalidEntity;

    // 次の兄弟。いないなら kInvalidEntity
    EntityID nextSibling = kInvalidEntity;
    
    // 前の兄弟
    EntityID prevSibling = kInvalidEntity;
};


