#include "HierarchySystem.h"

void HierarchySystem::Update(Registry& registry)
{
    // [BNS-Optimization] View を取得する際、非公開だった配列に直接アクセス
    auto& transforms = registry.GetArray<TransformComponent>();
    uint32_t size = transforms.GetSize();
    if (size == 0)
    {
        return;
    }

    // 階層情報（親子関係）がある場合の処理
    const bool hasHierarchy = registry.HasComponentArray<HierarchyComponent>();

    for (uint32_t i = 0; i < size; ++i)
    {
        EntityID entity = transforms.GetEntityFromDenseIndex(i);
        
        bool isRoot = true;
        if (hasHierarchy && registry.HasComponent<HierarchyComponent>(entity))
        {
            if (registry.GetComponent<HierarchyComponent>(entity).parent_ != kInvalidEntity)
            {
                isRoot = false;
            }
        }

        if (isRoot)
        {
            TransformComponent& transform = transforms.GetData(entity);

            // ルートが Dirty なら更新し、子へ伝播
            if (transform.isDirty_)
            {
                UpdateTransform(registry, entity);
                UpdateChildrenRecursive(registry, entity, true);
                transform.isDirty_ = false;
            }
            else
            {
                // 自分は動いていなくても、子が個別に動いている可能性があるため再帰は必要
                UpdateChildrenRecursive(registry, entity, false);
            }
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

void HierarchySystem::UpdateChildrenRecursive(Registry& registry, EntityID parentEntity, bool parentDirty)
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
        if (registry.HasComponent<TransformComponent>(currentChild))
        {
            TransformComponent& childTransform = registry.GetComponent<TransformComponent>(currentChild);
            
            bool needsUpdate = parentDirty || childTransform.isDirty_;

            if (needsUpdate)
            {
                // LocalMatrix を再計算（自分自身が変更された場合のみで本当は良いが、
                // 一旦親が動いた場合も再計算する安全側に倒す。本来は localDirty を分けるべき）
                UpdateTransform(registry, currentChild);

                // WorldMatrix を親と結合
                childTransform.worldMatrix_ = Multiply(childTransform.localMatrix_, parentTransform.worldMatrix_);
                
                // 次の子孫へ伝播（自分が動いたので、子も必ず更新が必要）
                UpdateChildrenRecursive(registry, currentChild, true);
                
                childTransform.isDirty_ = false;
            }
            else
            {
                // 自分も親も動いていない。
                // ただし、更にその孫が個別に動いている可能性があるため、parentDirty=false で探索継続。
                UpdateChildrenRecursive(registry, currentChild, false);
            }
        }

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
