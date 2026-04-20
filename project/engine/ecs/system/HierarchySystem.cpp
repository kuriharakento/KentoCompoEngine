#include "HierarchySystem.h"

void HierarchySystem::Update(Registry& registry)
{
    // [BNS-Optimization] View を取得する際、E??公開だった?E列に直接アクセス
    auto& transforms = registry.GetArray<TransformComponent>();
    uint32_t size = transforms.GetSize();
    if (size == 0)
    {
        return;
    }

    // 階層?E???E?親子関係）がある場合?E処?E
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
                // 自?E?E動いてぁE??くても、子が個別に動いてぁE??可能性があるため?E帰は?E??E
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

    // Local Matrix の構篁E
    transform.localMatrix_ = MakeAffineMatrix(
        transform.localScale_,
        transform.localRotation_,
        transform.localPosition_
    );

    // World Matrix の計?E
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
                // LocalMatrix を?E計算（?E?E?E身が変更された場合?Eみで本当?E良ぁE??、E
                // 一旦親が動ぁE??場合も再計算する安?E側に倒す。本来は localDirty を?Eけるべき！E
                UpdateTransform(registry, currentChild);

                // WorldMatrix を親と結合
                childTransform.worldMatrix_ = Multiply(childTransform.localMatrix_, parentTransform.worldMatrix_);
                
                // 次の子孫へ伝播?E??E?E??動いた?Eで、子も?E??更新が?E??E??E
                UpdateChildrenRecursive(registry, currentChild, true);
                
                childTransform.isDirty_ = false;
            }
            else
            {
                // 自?E??親も動ぁE??ぁE??ぁE??E
                // ただし、更にそ?E孫が個別に動いてぁE??可能性があるため、parentDirty=false で探索継続、E
                UpdateChildrenRecursive(registry, currentChild, false);
            }
        }

        // 次の允E??へ
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
