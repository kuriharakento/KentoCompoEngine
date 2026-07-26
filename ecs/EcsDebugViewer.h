#pragma once

#include "Registry.h"
#include <memory>

namespace KCE
{
// No namespaces

/**
 * @brief Registryの内部状態（生成Entity数、各プール容量など）をImGui上に可視化するデバッグツール。
 * 
 * 既存のゲームシステムを壊さずにECSへの移行状態を監視するために、
 * シーンのUpdateやDrawの末尾等で単独で呼び出すことを想定している。
 */
class EcsDebugViewer
{
public:
    EcsDebugViewer() = default;
    ~EcsDebugViewer() = default;

    /**
     * @brief 初期化
     */
    void Initialize();

    /**
     * @brief ImGuiのウィンドウを描画して、渡されたRegistryの内部数値を表示する。
     * @param registry 監視したい対象のRegistryオブジェクト
     */
    void DrawImGui(const Registry& registry);

private:
    // ウィンドウを表示するかどうかのフラグ
    bool m_showWindow = true;
};
} // namespace KCE
