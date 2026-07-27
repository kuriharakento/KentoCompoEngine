#include "InstancedRenderSystem.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/InstancedRenderComponent.h"
#include "graphics/3d/InstancedModelRenderer.h"
#include "base/Camera.h"
#include "manager/scene/LightManager.h"
#include "engine/manager/graphics/ShadowMapManager.h"

namespace KCE
{
using namespace ecs;

// 静的メンバの定義
std::vector<Matrix4x4> InstancedRenderSystem::s_instanceMatrices;
std::unordered_map<std::string, std::vector<Matrix4x4>> InstancedRenderSystem::s_groupedMatrices;
void InstancedRenderSystem::Update(Registry& registry)
{
    (void)registry;
}

void InstancedRenderSystem::Draw(Registry& registry, InstancedModelRenderer& renderer, Camera* camera, LightManager* lightManager, ShadowMapManager* shadowMapManager)
{
    // [BNS-Optimization] ハッシュマップ検索なしで配?Eを直接取征E
    auto& transforms = registry.GetArray<TransformComponent>();
    uint32_t activeCount = transforms.GetSize();
    if (activeCount == 0)
    {
        return;
    }

    // [BNS-Optimization] ワークバッファを?E利用?E?メモリ確保を 0 に?E?E
    s_instanceMatrices.clear();
    s_instanceMatrices.reserve(activeCount);
    
    // 全ての TransformComponent を収雁E
    for (uint32_t i = 0; i < activeCount; ++i)
    {
        // 直接 SoA 配?Eから Matrix を取征E
        s_instanceMatrices.push_back(transforms.GetDataFromDenseIndex(i).worldMatrix_);
    }

    if (s_instanceMatrices.empty())
    {
        return;
    }

    // GPUバッファに転送E
    renderer.UpdateBuffer(s_instanceMatrices.data(), static_cast<uint32_t>(s_instanceMatrices.size()), camera);

    // 描画実?E
    renderer.DrawInstanced(camera, lightManager, shadowMapManager);
}

void InstancedRenderSystem::DrawGrouped(
    Registry& registry,
    const std::unordered_map<std::string, std::unique_ptr<InstancedModelRenderer>>& renderers,
    Camera* camera,
    LightManager* lightManager,
    ShadowMapManager* shadowMapManager)
{
    // [BNS-Optimization] ハッシュマップ検索なしで配?Eを直接取征E
    auto& renders = registry.GetArray<InstancedRenderComponent>();
    uint32_t componentCount = renders.GetSize();
    if (componentCount == 0)
    {
        return;
    }

    // [BNS-Optimization] ワークバッファを?E利用。前回?EチE?Eタをクリア
    for (auto& [name, vec] : s_groupedMatrices)
    {
        vec.clear();
    }

    auto& transforms = registry.GetArray<TransformComponent>();

    for (uint32_t i = 0; i < componentCount; ++i)
    {
        EntityID entity = renders.GetEntityFromDenseIndex(i);
        const InstancedRenderComponent& render = renders.GetDataFromDenseIndex(i);

        // 描画対象外?EスキチE?E
        if (!render.isVisible_ || !render.useInstancing_)
        {
            continue;
        }

        if (transforms.HasComponent(entity))
        {
            const TransformComponent& transform = transforms.GetData(entity);
            s_groupedMatrices[render.modelName_].push_back(transform.worldMatrix_);
        }
    }

    // レンダラへ転送して描画
    for (auto& [modelName, matrices] : s_groupedMatrices)
    {
        if (matrices.empty()) continue;

        auto it = renderers.find(modelName);
        if (it == renderers.end() || it->second == nullptr)
        {
            continue;
        }

        it->second->UpdateBuffer(matrices.data(), static_cast<uint32_t>(matrices.size()), camera);
        it->second->DrawInstanced(camera, lightManager, shadowMapManager);
    }
}

void InstancedRenderSystem::DrawGBufferGrouped(
    Registry& registry,
    const std::unordered_map<std::string, std::unique_ptr<InstancedModelRenderer>>& renderers,
    Camera* camera)
{
    auto& renders = registry.GetArray<InstancedRenderComponent>();
    uint32_t componentCount = renders.GetSize();
    if (componentCount == 0)
    {
        return;
    }

    for (auto& [name, vec] : s_groupedMatrices)
    {
        vec.clear();
    }

    auto& transforms = registry.GetArray<TransformComponent>();

    for (uint32_t i = 0; i < componentCount; ++i)
    {
        EntityID entity = renders.GetEntityFromDenseIndex(i);
        const InstancedRenderComponent& render = renders.GetDataFromDenseIndex(i);

        if (!render.isVisible_ || !render.useInstancing_)
        {
            continue;
        }

        if (transforms.HasComponent(entity))
        {
            const TransformComponent& transform = transforms.GetData(entity);
            s_groupedMatrices[render.modelName_].push_back(transform.worldMatrix_);
        }
    }

    for (auto& [modelName, matrices] : s_groupedMatrices)
    {
        if (matrices.empty()) continue;

        auto it = renderers.find(modelName);
        if (it == renderers.end() || it->second == nullptr)
        {
            continue;
        }

        it->second->UpdateBuffer(matrices.data(), static_cast<uint32_t>(matrices.size()), camera);
        it->second->DrawInstancedGBuffer(camera);
    }
}

void InstancedRenderSystem::DrawShadowGrouped(
    Registry& registry,
    const std::unordered_map<std::string, std::unique_ptr<InstancedModelRenderer>>& renderers,
    Camera* camera,
    ShadowMapManager* shadowMapManager)
{
    auto& renders = registry.GetArray<InstancedRenderComponent>();
    uint32_t componentCount = renders.GetSize();
    if (componentCount == 0)
    {
        return;
    }

    for (auto& [name, vec] : s_groupedMatrices)
    {
        vec.clear();
    }

    auto& transforms = registry.GetArray<TransformComponent>();

    for (uint32_t i = 0; i < componentCount; ++i)
    {
        EntityID entity = renders.GetEntityFromDenseIndex(i);
        const InstancedRenderComponent& render = renders.GetDataFromDenseIndex(i);

        if (!render.isVisible_ || !render.useInstancing_)
        {
            continue;
        }

        if (transforms.HasComponent(entity))
        {
            const TransformComponent& transform = transforms.GetData(entity);
            s_groupedMatrices[render.modelName_].push_back(transform.worldMatrix_);
        }
    }

    for (auto& [modelName, matrices] : s_groupedMatrices)
    {
        if (matrices.empty()) continue;

        auto it = renderers.find(modelName);
        if (it == renderers.end() || it->second == nullptr)
        {
            continue;
        }

        it->second->UpdateBuffer(matrices.data(), static_cast<uint32_t>(matrices.size()), camera);
        it->second->DrawInstancedShadow(camera, shadowMapManager);
    }
}
} // namespace KCE
