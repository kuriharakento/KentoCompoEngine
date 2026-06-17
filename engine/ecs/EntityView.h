#pragma once

#include "Registry.h"
#include <tuple>

// No namespaces

/**
 * @brief 複数コンポーネントの組み合わせを高速に走査するためのビュークラス。
 *
 * [BNS-Standard] ループ内でのハッシュマップ検索を完全に排除し、
 * SoA (Structure of Arrays) の性能を最大限に引き出す。
 *
 * @tparam T コンポーネント型リスト
 */
template <typename... T>
class EntityView
{
public:
    /**
     * @brief コンストラクタ。内部で各コンポーネント配列の参照をキャッシュする。
     * @param registry 参照元のRegistry
     */
    EntityView(Registry& registry)
        : registry_(registry)
        , arrays_(std::tie(registry.GetArray<T>()...))
    {
    }

    /**
     * @brief ビューに含まれる全エンティティに対して関数を実行する。
     * @tparam Func 関数型
     * @param func 実行する関数。引数は (EntityID, T&...)
     */
    template <typename Func>
    void ForEach(Func&& func)
    {
        // 最初のコンポーネント配列（主キー）をベースにイテレーション
        auto& primaryArray = std::get<0>(arrays_);
        uint32_t size = primaryArray.GetSize();

        for (uint32_t i = 0; i < size; ++i)
        {
            EntityID entity = primaryArray.GetEntityFromDenseIndex(i);

            // 他のすべてのコンポーネントを保持しているかチェック
            if (HasAllComponents(entity))
            {
                // 関数実行
                func(entity, registry_.GetComponent<T>(entity)...);
            }
        }
    }

private:
    /**
     * @brief 全てのコンポーネントを保持しているか再帰的にチェック
     * @param entity 対象のEntityID
     * @return すべて持っていれば true
     */
    bool HasAllComponents(EntityID entity)
    {
        return (registry_.HasComponent<T>(entity) && ...);
    }

private:
    // 参照元のRegistry
    Registry& registry_;
    // キャッシュされたコンポーネント配列の参照タプル
    std::tuple<ComponentArray<T>&...> arrays_;
};
