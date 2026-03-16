#pragma once

#include <memory>
#include "engine/ecs/Registry.h"
#include "engine/ecs/EcsDebugViewer.h"
#include "engine/camerawork/debug/DebugCamera.h"
#include "engine/graphics/3d/InstancedModelRenderer.h"
#include "engine/scene/interface/BaseScene.h"

// 既存エンジンのインターフェース(Scene基底など)がある場合は継承してください
// #include "IScene.h"

// No namespaces

/**
 * @brief ECSアーキテクチャの機能や負荷を専属でテストするための独立したデバッグシーン。
 *
 * 既存の EnemyManager 等を壊さずに、万単位のEntity生成・破棄、
 * およびInstancing描画のストレステストを行うためのサンドボックスである。
 */
class EcsDebugScene : public BaseScene
{
public:
    EcsDebugScene();
    ~EcsDebugScene() override;

    /**
     * @brief シーンの初期化。Registryの構築やバッファの確保を行う
     */
    void Initialize() override;

    /**
     * @brief 終了処理
     */
    void Finalize() override;

    /**
     * @brief 更新処理
     */
    void CommonUpdate() override;

    /**
     * @brief 3D描画処理
     */
    void Draw3D() override;

    /**
     * @brief 2D描画処理
     */
    void Draw2D() override;

private:
    // --- ECS Core ---
    Registry registry_;
    std::unique_ptr<EcsDebugViewer> debugViewer_;

    // --- Systems & Renderers ---
    std::unique_ptr<InstancedModelRenderer> instancedRenderer_;

    // --- Debug Settings ---
    std::unique_ptr<DebugCamera> debugCamera_;

    // 1回押したときに生成するダミーEntityの数
    uint32_t spawnCountPerClick_ = 1000;
};
