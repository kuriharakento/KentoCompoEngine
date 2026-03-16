#include "InstancedRenderSystem.h"
#include "application/ecs/components/TransformComponent.h"
#include "application/ecs/components/RenderComponent.h"
#include "graphics/3d/InstancedModelRenderer.h"
#include "base/Camera.h"
#include "manager/scene/LightManager.h"

void InstancedRenderSystem::Update(Registry& registry)
{
    (void)registry;
}

void InstancedRenderSystem::Draw(Registry& registry, InstancedModelRenderer& renderer, Camera* camera, LightManager* lightManager, ShadowMapManager* shadowMapManager)
{
    auto transformView = registry.View<TransformComponent>();
    if (!transformView)
    {
        return;
    }

    uint32_t activeCount = transformView->GetSize();
    if (activeCount == 0)
    {
        return;
    }

    std::vector<Matrix4x4> instanceMatrices;
    instanceMatrices.reserve(activeCount);
    
    // 全ての TransformComponent を収集
    for (uint32_t i = 0; i < activeCount; ++i)
    {
        EntityID entity = transformView->GetEntityFromDenseIndex(i);
        const Matrix4x4& worldMatrix = transformView->GetData(entity).worldMatrix_;
        
        instanceMatrices.push_back(worldMatrix);
    }

    if (instanceMatrices.empty())
    {
        return;
    }

    // GPUバッファに転送
    renderer.UpdateBuffer(instanceMatrices.data(), static_cast<uint32_t>(instanceMatrices.size()), camera);

    // 描画実行
    renderer.DrawInstanced(camera, lightManager, shadowMapManager);
}

void InstancedRenderSystem::DrawGrouped(
    Registry& registry,
    const std::unordered_map<std::string, std::unique_ptr<InstancedModelRenderer>>& renderers,
    Camera* camera,
    LightManager* lightManager,
    ShadowMapManager* shadowMapManager)
{
    auto renderView = registry.View<RenderComponent>();
    if (!renderView)
    {
        return;
    }

    uint32_t componentCount = renderView->GetSize();
    if (componentCount == 0)
    {
        return;
    }

    // モデル名ごとに WorldMatrix をグルーピング
    std::unordered_map<std::string, std::vector<Matrix4x4>> groupedMatrices;

    for (uint32_t i = 0; i < componentCount; ++i)
    {
        EntityID entity = renderView->GetEntityFromDenseIndex(i);
        const RenderComponent& render = renderView->GetData(entity);

        // 描画対象外はスキップ
        if (!render.isVisible_ || !render.useInstancing_)
        {
            continue;
        }

        if (registry.HasComponent<TransformComponent>(entity))
        {
            const TransformComponent& transform = registry.GetComponent<TransformComponent>(entity);
            
            Matrix4x4 m;
            std::memcpy(&m, &transform.worldMatrix_, sizeof(Matrix4x4));
            groupedMatrices[render.modelName_].push_back(m);
        }
    }

    // レンダラへ転送して描画
    for (auto& [modelName, matrices] : groupedMatrices)
    {
        auto it = renderers.find(modelName);
        if (it == renderers.end() || it->second == nullptr)
        {
            continue;
        }

        it->second->UpdateBuffer(matrices.data(), static_cast<uint32_t>(matrices.size()), camera);
        it->second->DrawInstanced(camera, lightManager, shadowMapManager);
    }
}
