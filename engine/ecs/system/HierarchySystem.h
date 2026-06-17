#pragma once


#include "ISystem.h"
#include "../../../engine/ecs/Registry.h"
#include "../components/TransformComponent.h"
#include "../components/HierarchyComponent.h"

/**
 * @brief 親子関係を持つEntityのWorldMatrixを計算するシスチE??
 */
class HierarchySystem : public ISystem
{
public:
    /**
     * @brief WorldMatrix を更新する
     * @param registry 対象のRegistry
     */
    void Update(Registry& registry) override;

private:
    /**
     * @brief 持E??EntityのLocalMatrixおよびWorldMatrixを更新する
     */
    static void UpdateTransform(Registry& registry, EntityID entity);

    /**
     * @brief Entityの子孫に対して再帰?E??WorldMatrix計算を伝播させめE
     */
    void UpdateChildrenRecursive(Registry& registry, EntityID parentEntity, bool parentDirty);
};
