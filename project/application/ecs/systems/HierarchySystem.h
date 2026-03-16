#pragma once

#include "../../../engine/ecs/Registry.h"
#include "../components/TransformComponent.h"
#include "../components/HierarchyComponent.h"

/**
 * @brief 親子関係（階層構造）を持つEntityのWorldMatrixを計算するシステム。
 * 
 * Updateフェーズの早い段階で呼ばれる事を想定。
 * TransformSystem（全Entityを一律に計算するシステム）の代替、もしくは
 * 親子を持たない物と持つ物で分けて処理するアーキテクチャの一部として機能する。
 */
class HierarchySystem
{
public:
    /**
     * @brief レジストリ内の全ての HierarchyComponent を走査し、WorldMatrix を更新する。
     * @param registry 対象のRegistry
     */
    static void Update(Registry& registry);

private:
    /**
     * @brief 指定EntityのLocalMatrixおよびWorldMatrixを更新する内部処理
     */
    static void UpdateTransform(Registry& registry, EntityID entity);

    /**
     * @brief Entityの子孫に対して再帰的にWorldMatrix計算を伝播させる
     */
    static void UpdateChildrenRecursive(Registry& registry, EntityID parentEntity);
};
