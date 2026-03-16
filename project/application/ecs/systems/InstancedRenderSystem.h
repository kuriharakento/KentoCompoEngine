#pragma once

#include "../../../engine/ecs/Registry.h"
#include "../../../engine/graphics/3d/InstancedModelRenderer.h"
#include "../components/TransformComponent.h"
#include "../components/RenderComponent.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <string>

class Camera;
class LightManager;

/**
 * @brief 描画すべきEntity群のTransformを集約し、InstancedModelRendererへ転送するシステム。
 *
 * Draw: 単一レンダラへ全TransformComponentを流す（シンプル版、ストレステスト用）
 * DrawGrouped: RenderComponent.modelName でグルーピングし、モデル別レンダラマップへ転送する版
 */
class InstancedRenderSystem
{
public:
    static void Update(Registry& registry);

    /**
     * @brief 全TransformComponentを1つのレンダラへ転送して描画する（シンプル版）。
     * @param registry 対象のRegistry
     * @param renderer 描画先インスタンシングレンダラ
     * @param camera 使用するカメラ
     * @param lightManager ライトマネージャー
     * @param shadowMapManager シャドウマップマネージャー
     */
    static void Draw(Registry& registry, InstancedModelRenderer& renderer, Camera* camera, LightManager* lightManager, class ShadowMapManager* shadowMapManager);

    /**
     * @brief RenderComponent.modelName でEntityをグルーピングし、モデル別レンダラへ転送する。
     * @param registry 対象のRegistry
     * @param renderers モデル名 → InstancedModelRenderer のマップ（呼び出し元が管理）
     * @param camera 使用するカメラ
     * @param lightManager ライトマネージャー
     * @param shadowMapManager シャドウマップマネージャー
     */
    static void DrawGrouped(
        Registry& registry,
        const std::unordered_map<std::string, std::unique_ptr<InstancedModelRenderer>>& renderers,
        Camera* camera,
        LightManager* lightManager,
        class ShadowMapManager* shadowMapManager
    );
};
