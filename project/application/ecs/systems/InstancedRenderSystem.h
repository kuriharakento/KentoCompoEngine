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

class InstancedRenderSystem
{
public:
    /**
     * @brief 事前計算などを行う
     */
    static void Update(Registry& registry);

    /**
     * @brief 全TransformComponentを1つのレンダラへ転送して描画する
     * @param registry 対象のRegistry
     * @param renderer 描画先インスタンシングレンダラ
     * @param camera 使用するカメラ
     * @param lightManager ライトマネージャー
     * @param shadowMapManager シャドウマップマネージャー
     */
    static void Draw(Registry& registry, InstancedModelRenderer& renderer, Camera* camera, LightManager* lightManager, class ShadowMapManager* shadowMapManager);

    /**
     * @brief モデル名でグルーピングして描画する
     * @param registry 対象のRegistry
     * @param renderers モデル名 → レンダラのマップ
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
