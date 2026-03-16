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

    /** @brief シーンの初期化。Registryの構築やバッファの確保を行う */
    void Initialize() override;
    void Finalize() override;
    void CommonUpdate() override;
    void Draw3D() override;
    void Draw2D() override;

private:
    // --- ECS Core（値で保有。shared_ptr不要）---
    Registry m_registry;
    std::unique_ptr<EcsDebugViewer> m_debugViewer;

    // --- Systems & Renderers ---
    std::unique_ptr<InstancedModelRenderer> m_instancedRenderer;

    // --- Debug Settings ---
    std::unique_ptr<DebugCamera> m_debugCamera;
    uint32_t m_spawnCountPerClick = 1000; //!< 1回押したときに生成するダミーEntityの数
};
