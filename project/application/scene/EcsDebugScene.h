#pragma once

#include <memory>
#include "engine/ecs/Registry.h"
#include "engine/ecs/debug/EcsInspector.h"
#include "engine/camerawork/debug/DebugCamera.h"
#include "engine/graphics/3d/InstancedModelRenderer.h"
#include "engine/scene/interface/BaseScene.h"
#include "engine/ecs/system/SystemManager.h"

/**
 * @brief ECSのストレステスト用デバッグシーン。
 */
class EcsDebugScene : public BaseScene
{
public:
    EcsDebugScene();
    ~EcsDebugScene() override;

    /**
     * @brief 初期化。
     */
    void Initialize() override;

    /**
     * @brief 終了処理。
     */
    void Finalize() override;

    void DrawImGui();

    /**
     * @brief 更新処理。
     */
    void CommonUpdate() override;

    /**
     * @brief 3D描画。
     */
    void Draw3D() override;

    /**
     * @brief 2D描画。
     */
    void Draw2D() override;

private:
    // --- ECS Core ---
    std::unique_ptr<Registry> registry_;
    std::unique_ptr<EcsInspector> inspector_;
    std::unique_ptr<SystemManager> systemManager_;

    // --- Renderers ---
    std::unique_ptr<InstancedModelRenderer> instancedRenderer_;

    // --- Debug Settings ---
    std::unique_ptr<DebugCamera> debugCamera_;

    // 生成数設定
    uint32_t spawnCountPerClick_ = 1000;
};

