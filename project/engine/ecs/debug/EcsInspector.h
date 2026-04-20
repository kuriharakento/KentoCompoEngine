#pragma once

#include "../Registry.h"
#include <string>
#include <vector>

// No namespaces

/**
 * @brief ECSの状態を詳細に監視・編集するためのデバッグインスペクター。
 * [BNS-Standard] 実行時のパフォーマンスだけでなく、開発効率を最大化するためのツール。
 */
class EcsInspector
{
public:
    EcsInspector() = default;
    ~EcsInspector() = default;

    /**
     * @brief 初期化処理
     */
    void Initialize();

    /**
     * @brief インスペクターウィンドウを描画する。
     * @param registry 監視対象のRegistry
     */
    void Draw(Registry& registry);

private:
    /**
     * @brief エンティティリストペインを描画
     */
    void DrawEntityList(Registry& registry);

    /**
     * @brief コンポーネント編集ペインを描画
     */
    void DrawComponentEditor(Registry& registry, EntityID entity);

private:
    // ウィンドウを表示するか
    bool showWindow_ = true;
    // 現在選択されているエンティティ
    EntityID selectedEntity_ = 0xFFFFFFFF; // kInvalidEntity
    // 検索フィルタ文字列
    char searchFilter_[128] = "";
};
