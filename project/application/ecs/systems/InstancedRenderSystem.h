#pragma once

#include "../../../engine/ecs/Registry.h"
#include "../../../engine/graphics/3d/InstancedModelRenderer.h"
#include "../components/TransformComponent.h"
#include "../components/RenderComponent.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <string>

#include "../../../engine/math/MatrixFunc.h"

class Camera;
class LightManager;

class InstancedRenderSystem
{
public:
    // ... 前略 ...
    static void Update(Registry& registry);
    static void Draw(Registry& registry, InstancedModelRenderer& renderer, Camera* camera, LightManager* lightManager, class ShadowMapManager* shadowMapManager);
    static void DrawGrouped(
        Registry& registry,
        const std::unordered_map<std::string, std::unique_ptr<InstancedModelRenderer>>& renderers,
        Camera* camera,
        LightManager* lightManager,
        class ShadowMapManager* shadowMapManager
    );

private:
    // [BNS-Optimization] 毎フレームの動的確保を避けるためのワークバッファ
    static std::vector<Matrix4x4> s_instanceMatrices;
    static std::unordered_map<std::string, std::vector<Matrix4x4>> s_groupedMatrices;
};
