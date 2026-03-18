#include "Object3dSystem.h"
#include "../components/Object3dComponent.h"
#include "../components/TransformComponent.h"
#include "../../../engine/graphics/3d/Object3d.h"

void Object3dSystem::Draw(Registry& registry, Camera* camera)
{
    // 対象コンポーネントが無ければスキップ
    if (!registry.HasComponentArray<Object3dComponent>())
    {
        return;
    }

    auto& objectArrays = registry.GetArray<Object3dComponent>();
    uint32_t count = objectArrays.GetSize();
    if (count == 0)
    {
        return;
    }

    // Transformを持っているか確認しておく
    bool hasTransform = registry.HasComponentArray<TransformComponent>();
    auto* transforms = hasTransform ? &registry.GetArray<TransformComponent>() : nullptr;

    // まとめて描画を回す
    for (uint32_t i = 0; i < count; ++i)
    {
        EntityID entity = objectArrays.GetEntityFromDenseIndex(i);
        const Object3dComponent& objComp = objectArrays.GetDataFromDenseIndex(i);

        // 非表示や実体がない場合はスキップ
        if (!objComp.isVisible_ || !objComp.object3d_)
        {
            continue;
        }

        // ECSでTransform管理されていればワールド行列を渡し、なければ単独更新する
        if (transforms && transforms->HasComponent(entity))
        {
            const TransformComponent& transform = transforms->GetData(entity);
            objComp.object3d_->UpdateMatrixWithWorld(transform.worldMatrix_, camera);
        }
        else
        {
            objComp.object3d_->UpdateMatrix(camera);
        }

        objComp.object3d_->Draw();
    }
}
