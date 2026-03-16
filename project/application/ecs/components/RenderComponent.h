#pragma once

#include <string>

// No namespaces

/**
 * @brief 描画に必要な情報を保持するコンポーネント。
 *
 * InstancedRenderSystem はこのコンポーネントの modelName を
 * グルーピングキーとして、モデル種別ごとに一括描画（インスタンシング）を行う。
 */
struct RenderComponent
{
    // 描画するモデルの名前（InstancedModelRenderer のキーとして使う）
    std::string modelName;

    // true なら InstancedRenderSystem による一括描画対象とする
    bool useInstancing = true;

    // false なら描画をスキップ（カリングや非表示用）
    bool isVisible = true;
};
