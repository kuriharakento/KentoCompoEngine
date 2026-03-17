#include "InstancedRenderSystem.h"
#include "application/ecs/components/TransformComponent.h"
#include "application/ecs/components/RenderComponent.h"
#include "graphics/3d/InstancedModelRenderer.h"
#include "base/Camera.h"
#include "manager/scene/LightManager.h"

// 静的メンバの定義
std::vector<Matrix4x4> InstancedRenderSystem::s_instanceMatrices;
std::unordered_map<std::string, std::vector<Matrix4x4>> InstancedRenderSystem::s_groupedMatrices;

void InstancedRenderSystem::Update(Registry& registry)
{
    (void)registry;
}

void InstancedRenderSystem::Draw(Registry& registry, InstancedModelRenderer& renderer, Camera* camera, LightManager* lightManager, ShadowMapManager* shadowMapManager)
{
    // [BNS-Optimization] ハッシュマップ検索なしで配列を直接取得
    auto& transforms = registry.GetArray<TransformComponent>();
    uint32_t activeCount = transforms.GetSize();
    if (activeCount == 0)
    {
        return;
    }

    // [BNS-Optimization] ワークバッファを再利用（メモリ確保を 0 に）
    s_instanceMatrices.clear();
    s_instanceMatrices.reserve(activeCount);
    
    // 全ての TransformComponent を収集
    for (uint32_t i = 0; i < activeCount; ++i)
    {
        // 直接 SoA 配列から Matrix を取得
        s_instanceMatrices.push_back(transforms.GetDataFromDenseIndex(i).worldMatrix_);
    }

    if (s_instanceMatrices.empty())
    {
        return;
    }

    // GPUバッファに転送
    renderer.UpdateBuffer(s_instanceMatrices.data(), static_cast<uint32_t>(s_instanceMatrices.size()), camera);

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
    // [BNS-Optimization] ハッシュマップ検索なしで配列を直接取得
    auto& renders = registry.GetArray<RenderComponent>();
    uint32_t componentCount = renders.GetSize();
    if (componentCount == 0)
    {
        return;
    }

    // [BNS-Optimization] ワークバッファを再利用。前回のデータをクリア
    for (auto& [name, vec] : s_groupedMatrices)
    {
        vec.clear();
    }

    auto& transforms = registry.GetArray<TransformComponent>();

    for (uint32_t i = 0; i < componentCount; ++i)
    {
        EntityID entity = renders.GetEntityFromDenseIndex(i);
        const RenderComponent& render = renders.GetDataFromDenseIndex(i);

        // 描画対象外はスキップ
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
