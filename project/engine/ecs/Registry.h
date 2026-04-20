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
     * @brief 新しいEntityを生成する。(ラッパーを返す推奨API)
     * @return Entityラッパーオブジェクト
     */
    Entity Create();

    /**
     * @brief 新しいEntityを生成する。(従来の生IDを返すAPI)
     * @return プールに空きがあれば生成されたEntityID。枯渇時は kInvalidEntity。
     */
    EntityID CreateEntity();

    /**
     * @brief Entityが有効（現在生きているか）を判定する。
     * @param entity 対象のEntityID
     * @return 生きていれば true。
     */
    bool IsAlive(EntityID entity) const;

    /**
     * @brief エンティティの破棄をフレーム終端まで予約する。
     * @param entity 破棄したいEntityID
     */
    void DestroyEntityDeferred(EntityID entity);

    /**
     * @brief 予約されたEntityと関連Componentを一括削除する。
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
        assert(componentArrays_.find(typeName) == componentArrays_.end() && "Component already registered.");

        componentArrays_.insert({ typeName, std::make_shared<ComponentArray<T>>(maxEntities_, maxComponents) });
    }

    /**
     * @brief Entityにコンポーネントを追加する。
     * @tparam T 追加するコンポーネントの型
     * @param entity 対象のエンティティ
     * @param component 初期データ
     * @param policy キャパシティオーバー時の処理
     * @return 成功したか
     */
    template <typename T>
    bool AddComponent(EntityID entity, T component, PoolExhaustionPolicy policy = PoolExhaustionPolicy::AssertAndCrash)
    {
        assert(IsAlive(entity) && "Cannot add component to a dead entity.");
        auto array = GetComponentArray<T>();
        assert(array && "Component not registered.");
        return array->Insert(entity, std::move(component), policy);
    }

    /**
     * @brief Entityが持つコンポーネントを削除する（即時）。
     * @param entity 対象のエンティティ
     */
    template <typename T>
    void RemoveComponent(EntityID entity)
    {
        if (IsAlive(entity))
        {
            auto array = GetComponentArray<T>();
            assert(array && "Component not registered.");
            array->Remove(entity);
        }
    }

    /**
     * @brief Entityから特定のコンポーネントの参照を取得する。
     * @param entity 対象のエンティティ
     * @return コンポーネントへの参照
     */
    template <typename T>
    T& GetComponent(EntityID entity)
    {
        assert(IsAlive(entity) && "Cannot get component from a dead entity.");
        auto array = GetComponentArray<T>();
        assert(array && "Component not registered.");
        return array->GetData(entity);
    }

    /**
     * @brief Entityが特定のコンポーネントを持っているか判定する。
     * @param entity 対象のエンティティ
     * @return 持っていれば true
     */
    template <typename T>
    bool HasComponent(EntityID entity)
    {
        if (!IsAlive(entity))
        {
            return false;
        }
        auto array = GetComponentArray<T>();
        if (!array) return false;
        return array->HasComponent(entity);
    }

    // =========================================================================
    // System Integration API (High-Performance)
    // =========================================================================

    /**
     * @brief ComponentArray を直接取得する。
     * @tparam T コンポーネント型
     * @return ComponentArrayへの参照
     */
    template <typename T>
    ComponentArray<T>& GetArray()
    {
        std::type_index typeName = std::type_index(typeid(T));
        assert(componentArrays_.find(typeName) != componentArrays_.end() && "Component not registered.");
        return *std::static_pointer_cast<ComponentArray<T>>(componentArrays_[typeName]);
    }

    /**
     * @brief 指定したコンポーネント型が登録されているか判定する。
     * @return 登録済みなら true
     */
    template <typename T>
    bool HasComponentArray() const
    {
        return componentArrays_.find(std::type_index(typeid(T))) != componentArrays_.end();
    }

    /**
     * @brief 指定したコンポーネント配列全体を走査するためのビューを取得する。
     * @tparam T コンポーネント型
     * @return ComponentArrayを格納したshared_ptr
     */
    template <typename T>
    std::shared_ptr<ComponentArray<T>> View()
    {
        std::type_index typeName = std::type_index(typeid(T));
        if (componentArrays_.find(typeName) == componentArrays_.end()) {
            FILE* f;
            fopen_s(&f, "missing_component_log.txt", "w");
            if (f) {
                fprintf(f, "Missing component: %s\n", typeName.name());
                fclose(f);
            }
        }
        assert(componentArrays_.find(typeName) != componentArrays_.end() && "Component not registered.");
        return std::static_pointer_cast<ComponentArray<T>>(componentArrays_[typeName]);
    }

    // =========================================================================
    // Debug & Metrics API
    // =========================================================================

    /**
     * @brief 生きているEntity数を取得
     */
    uint32_t GetActiveEntityCount() const { return activeEntityCount_; }

    /**
     * @brief 最大Entityキャパシティを取得
     */
    uint32_t GetMaxEntityCount() const { return maxEntities_; }

private:
    /**
     * @brief 内部的なComponentArray取得
     */
    template <typename T>
    std::shared_ptr<ComponentArray<T>> GetComponentArray()
    {
        std::type_index typeName = std::type_index(typeid(T));
        auto it = componentArrays_.find(typeName);
        if (it == componentArrays_.end())
        {
            return nullptr;
        }
        return std::static_pointer_cast<ComponentArray<T>>(it->second);
    }

private:
    // 最大Entity数
    uint32_t maxEntities_ = 0;
    // 現在の生存Entity数
    uint32_t activeEntityCount_ = 0;

    // 世代管理用（index -> generation）
    std::vector<uint32_t> generations_;
    // リサイクル可能なIndexキュー
    std::queue<uint32_t> availableIndices_;

    // 各コンポーネント型と、それを管理するComponentArray群へのポインタ
    std::unordered_map<std::type_index, std::shared_ptr<IComponentArray>> componentArrays_;

    // フレーム末尾の破棄実行（Flush）を待つ予約キュー
    std::vector<EntityID> destroyQueue_;
};

// =========================================================================
// Entityラッパーの実装（テンプレート）
// =========================================================================

template<typename T>
inline Entity& Entity::Add(T component)
{
    assert(IsValid() && "Cannot add component to an invalid/dead entity.");
    registry_->AddComponent<T>(id_, std::move(component));
    return *this;
}

template<typename T>
inline T& Entity::Get()
{
    assert(IsValid() && "Cannot get component from an invalid/dead entity.");
    return registry_->GetComponent<T>(id_);
}

template<typename T>
inline bool Entity::Has() const
{
    if (!IsValid()) return false;
    return registry_->HasComponent<T>(id_);
}

template<typename T>
inline void Entity::Remove()
{
    if (IsValid()) {
        registry_->RemoveComponent<T>(id_);
    }
}

inline void Entity::Destroy()
{
    if (IsValid()) {
        registry_->DestroyEntityDeferred(id_);
    }
}

inline bool Entity::IsValid() const
{
    return registry_ != nullptr && registry_->IsAlive(id_);
}
