#pragma once

#include "Entity.h"
#include "ComponentArray.h"
#include <queue>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <cassert>
#include <iostream>

// Deleted kento_compo and ecs namespaces

/**
 * @brief Entityと各ComponentArrayを統括するシステムコア。
 * 
 * - Entityの発行とリサイクル（世代管理つき）
 * - 任意のコンポーネント配列へのアクセス提供
 * - Systemループ中での安全な破棄を実現する遅延削除（Deferred Deletion）
 */
class Registry
{
public:
    Registry() = default;
    ~Registry() = default;

    /**
     * @brief システムの最大Entity数を設定し、各種事前割り当て（Pre-allocation）を行う。
     * @param maxEntities 動作させたい最大エンティティ数
     */
    void Initialize(uint32_t maxEntities);

    // =========================================================================
    // Entity Lifecycle API
    // =========================================================================

    /**
     * @brief 新しいEntityを生成する。
     * @return プールに空きがあれば生成されたEntityID。枯渇時は kInvalidEntity。
     */
    EntityID CreateEntity();

    /**
     * @brief Entityが有効（現在生きているか）を判定する。
     * @param entity 対象のEntityID
     * @return 生きていれば true。別世代が再利用中、または破棄済みなら false。
     */
    bool IsAlive(EntityID entity) const;

    /**
     * @brief [安全] エンティティの破棄をただちに実行せず、フレーム終端まで予約する。
     * ※Systemのループ処理中に配列のSwap&Popが発火してイテレータが壊れるのを防ぐためのメインAPI。
     * @param entity 破棄したいEntityID
     */
    void DestroyEntityDeferred(EntityID entity);

    /**
     * @brief フレームの最後に1回だけ呼ばれ、予約されたEntityと関連Componentを一括削除する。
     */
    void FlushGarbageCollection();

    // =========================================================================
    // Component API
    // =========================================================================

    /**
     * @brief 対象のComponentのマネージャー（ComponentArray）を作成・登録する。
     * @tparam T コンポーネント型
     * @param maxComponents このコンポーネントを同時に持つことができる最大数
     */
    template <typename T>
    void RegisterComponent(uint32_t maxComponents)
    {
        std::type_index typeName = std::type_index(typeid(T));
        assert(m_componentArrays.find(typeName) == m_componentArrays.end() && "Component already registered.");

        m_componentArrays.insert({typeName, std::make_shared<ComponentArray<T>>(m_maxEntities, maxComponents)});
    }

    /**
     * @brief Entityにコンポーネントを追加する。
     * @tparam T 追加するコンポーネントの型
     * @param entity 対象のエンティティ
     * @param component 初期データ
     * @param policy キャパシティオーバー時の処理（デフォルトはAssertで落とす）
     * @return 成功したか
     */
    template <typename T>
    bool AddComponent(EntityID entity, T component, PoolExhaustionPolicy policy = PoolExhaustionPolicy::AssertAndCrash)
    {
        assert(IsAlive(entity) && "Cannot add component to a dead entity.");
        return GetComponentArray<T>()->Insert(entity, component, policy);
    }

    /**
     * @brief Entityが持つコンポーネントを削除する（即時）。
     * ※これをSystemのUpdateループ中に呼ぶとイテレータが壊れるリスクがあるので注意。
     */
    template <typename T>
    void RemoveComponent(EntityID entity)
    {
        if (IsAlive(entity))
        {
            GetComponentArray<T>()->Remove(entity);
        }
    }

    /**
     * @brief Entityから特定のコンポーネントの参照を取得する。
     */
    template <typename T>
    T& GetComponent(EntityID entity)
    {
        assert(IsAlive(entity) && "Cannot get component from a dead entity.");
        return GetComponentArray<T>()->GetData(entity);
    }

    /**
     * @brief Entityが特定のコンポーネントを持っているか判定する。
     */
    template <typename T>
    bool HasComponent(EntityID entity)
    {
        if (!IsAlive(entity)) return false;
        return GetComponentArray<T>()->HasComponent(entity);
    }

    // =========================================================================
    // System Integration API
    // =========================================================================

    /**
     * @brief 指定したコンポーネント配列全体を走査するためのビューを取得する。
     * @tparam T 取得したいコンポーネントの型
     * @return その型のComponentArrayを返す
     * 
     * [使用例]
     * auto view = registry.View<TransformComponent>();
     * for(auto& transform : view) { ... }
     */
    template <typename T>
    std::shared_ptr<ComponentArray<T>> View()
    {
        return GetComponentArray<T>();
    }

    // =========================================================================
    // Debug & Metrics API
    // =========================================================================
    uint32_t GetActiveEntityCount() const { return m_activeEntityCount; }
    uint32_t GetMaxEntityCount() const { return m_maxEntities; }

private:
    /**
     * @brief 登録済みのComponentArrayをキャストして取得する内部関数。
     */
    template <typename T>
    std::shared_ptr<ComponentArray<T>> GetComponentArray()
    {
        std::type_index typeName = std::type_index(typeid(T));
        assert(m_componentArrays.find(typeName) != m_componentArrays.end() && "Component not registered before use.");

        return std::static_pointer_cast<ComponentArray<T>>(m_componentArrays[typeName]);
    }

private:
    uint32_t m_maxEntities = 0;
    uint32_t m_activeEntityCount = 0;

    // 世代管理用（index -> generation）
    std::vector<uint32_t> m_generations;

    // リサイクル可能なIndexキュー
    std::queue<uint32_t> m_availableIndices;

    // 各コンポーネント型と、それを管理するComponentArray群へのポインタ
    std::unordered_map<std::type_index, std::shared_ptr<IComponentArray>> m_componentArrays;

    // フレーム末尾の破棄実行（Flush）を待つ予約キュー
    std::vector<EntityID> m_destroyQueue;
};


