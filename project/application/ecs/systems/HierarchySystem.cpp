#include "HierarchySystem.h"

void HierarchySystem::Update(Registry& registry)
{
    // 1. 全ての TransformComponent を抽出する
    auto transformView = registry.View<TransformComponent>();
    if (!transformView || transformView->GetSize() == 0) return;

    for (uint32_t i = 0; i < transformView->GetSize(); ++i)
    {
        EntityID entity = transformView->GetEntityFromDenseIndex(i);
        
        // 階層情報がない、もしくはルート（親がいない）である場合のみ処理を開始する
        // これにより、ツリーの根本から順番に WorldMatrix が確定していく
        bool isRoot = true;
        if (registry.HasComponent<HierarchyComponent>(entity))
        {
            if (registry.GetComponent<HierarchyComponent>(entity).parent != kInvalidEntity)
            {
                isRoot = false;
            }
        }

        if (isRoot)
        {
            // 自分のトランスフォームを更新
            UpdateTransform(registry, entity);
            
            // 子へ再帰的に伝播させる
            UpdateChildrenRecursive(registry, entity);
        }
    }
}

void HierarchySystem::UpdateTransform(Registry& registry, EntityID entity)
{
    if (!registry.HasComponent<TransformComponent>(entity)) return;

    TransformComponent& transform = registry.GetComponent<TransformComponent>(entity);

    // 1. Local Matrix の構築 (Scale * Rotate * Translate)
    // MakeAffineMatrix は内部で Vector3 を引数に取る
    transform.localMatrix = MakeAffineMatrix(
        transform.localScale,
        transform.localRotation,
        transform.localPosition
    );

    // 2. World Matrix の計算
    // 親がいない場合は World = Local
    transform.worldMatrix = transform.localMatrix;
}

void HierarchySystem::UpdateChildrenRecursive(Registry& registry, EntityID parentEntity)
{
    if (!registry.HasComponent<HierarchyComponent>(parentEntity) ||
        !registry.HasComponent<TransformComponent>(parentEntity))
    {
        return;
    }

    const HierarchyComponent& parentHierarchy = registry.GetComponent<HierarchyComponent>(parentEntity);
    const TransformComponent& parentTransform = registry.GetComponent<TransformComponent>(parentEntity);

    EntityID currentChild = parentHierarchy.firstChild;

    while (currentChild != kInvalidEntity)
    {
        // 子のTransformを更新 (まずはLocalを計算)
        UpdateTransform(registry, currentChild);

        if (registry.HasComponent<TransformComponent>(currentChild))
        {
            TransformComponent& childTransform = registry.GetComponent<TransformComponent>(currentChild);

            // child.worldMatrix = child.localMatrix * parent.worldMatrix
            // Multiply は内部で Matrix4x4 を引数に取る
            childTransform.worldMatrix = Multiply(childTransform.localMatrix, parentTransform.worldMatrix);
        }

        // さらに下の孫へ伝播
        UpdateChildrenRecursive(registry, currentChild);

        // 次の兄弟へ
        if (registry.HasComponent<HierarchyComponent>(currentChild))
        {
            currentChild = registry.GetComponent<HierarchyComponent>(currentChild).nextSibling;
        }
        else
        {
            break; 
        }
    }
}
