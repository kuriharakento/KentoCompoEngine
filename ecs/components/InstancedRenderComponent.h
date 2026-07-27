#pragma once

#include <string>

namespace KCE
{
namespace ecs
{
    /**
     * @brief 描画に必要な情報を保持するコンポーネント。
     *
     * InstancedRenderSystem はこのコンポーネントの modelName を
     * グルーピングキーとして、モデル種別ごとに一括描画（インスタンシング）を行う。
     */
    struct InstancedRenderComponent
    {
        // モデル名
        std::string modelName_;

        // インスタンシングを使用するか
        bool useInstancing_ = true;

        // 表示フラグ
        bool isVisible_ = true;
    };
}
} // namespace KCE
