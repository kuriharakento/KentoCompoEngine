#include "HierarchySystem.h"

void HierarchySystem::Update(Registry& registry)
{
    // 全ての TransformComponent を抽出
    auto transformView = registry.View<TransformComponent>();
    if (!transformView || transformView->GetSize() == 0)
    {
        return;
    }

    for (uint32_t i = 0; i < transformView->GetSize(); ++i)
    {
        EntityID entity = transformView->GetEntityFromDenseIndex(i);
        
        // 階層情報がない、もしくはルート（親がいない）である場合のみ処理を開始
        // ツリーの根本から順番に WorldMatrix を計算していく
        bool isRoot = true;
        if (registry.HasComponent<HierarchyComponent>(entity))
        {
            if (registry.GetComponent<HierarchyComponent>(entity).parent_ != kInvalidEntity)
            {
                isRoot = false;
            }
        }

        if (isRoot)
        {
            // トランスフォームを更新
            UpdateTransform(registry, entity);
            
            // 子へ再帰的に伝播
            UpdateChildrenRecursive(registry, entity);
        }
    }
}

void HierarchySystem::UpdateTransform(Registry& registry, EntityID entity)
{
    if (!registry.HasComponent<TransformComponent>(entity))
    {
        return;
    }

    TransformComponent& transform = registry.GetComponent<TransformComponent>(entity);

    // Local Matrix の構築
    transform.localMatrix_ = MakeAffineMatrix(
        transform.localScale_,
        transform.localRotation_,
        transform.localPosition_
    );

    // World Matrix の計算
    transform.worldMatrix_ = transform.localMatrix_;
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

    EntityID currentChild = parentHierarchy.firstChild_;

    while (currentChild != kInvalidEntity)
    {
        // 子のTransformを更新
        UpdateTransform(registry, currentChild);

        if (registry.HasComponent<TransformComponent>(currentChild))
        {
            TransformComponent& childTransform = registry.GetComponent<TransformComponent>(currentChild);

            // WorldMatrix を親と結合
            childTransform.worldMatrix_ = Multiply(childTransform.localMatrix_, parentTransform.worldMatrix_);
        }

        // 下孫へ伝播
        UpdateChildrenRecursive(registry, currentChild);

        // 次の兄弟へ
        if (registry.HasComponent<HierarchyComponent>(currentChild))
        {
            currentChild = registry.GetComponent<HierarchyComponent>(currentChild).nextSibling_;
        }
        else
        {
            break; 
        }
    }
}
