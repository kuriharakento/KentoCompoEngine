#pragma once

#include "../../../engine/ecs/Registry.h"
#include "../components/TransformComponent.h"
#include "../components/HierarchyComponent.h"

/**
 * @brief 親子関係を持つEntityのWorldMatrixを計算するシステム
 */
class HierarchySystem
{
public:
    /**
     * @brief WorldMatrix を更新する
     * @param registry 対象のRegistry
     */
    static void Update(Registry& registry);

private:
    /**
     * @brief 指定EntityのLocalMatrixおよびWorldMatrixを更新する
     */
    static void UpdateTransform(Registry& registry, EntityID entity);

    /**
     * @brief Entityの子孫に対して再帰的にWorldMatrix計算を伝播させる
     */
    static void UpdateChildrenRecursive(Registry& registry, EntityID parentEntity, bool parentDirty);
};
