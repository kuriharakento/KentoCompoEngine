#pragma once

#include "Entity.h"
#include "ComponentArray.h"
#include <queue>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <cassert>

/**
 * @brief Entityとコンポーネントを管理するコアクラス。
 * 
 * - Entityの発行とリサイクル（世代管理）
 * - コンポーネント配列へのアクセス
 * - 遅延削除（Deferred Deletion）による安全な破棄
 */
class Registry
{
public:
    Registry() = default;
    ~Registry() = default;

    /**
     * @brief 初期化。最大Entity数を設定しメモリを確保する。
     * @param maxEntities 最大エンティティ数
     */
    void Initialize(uint32_t maxEntities);

    // --- Entity Lifecycle ---

    /**
     * @brief 新しいEntityを生成する（推奨API）。
     * @return Entityラッパー
     */
    Entity Create();

    /**
     * @brief 新しいEntityIDを発行する。
     * @return 生成されたEntityID。枯渇時は kInvalidEntity。
     */
    EntityID CreateEntity();

    /**
     * @brief Entityが生存しているか。
     * @param entity EntityID
     */
    bool IsAlive(EntityID entity) const;

    /**
     * @brief Entityの破棄を予約する（フレーム末尾で実行）。
     * @param entity 破棄対象のEntityID
     */
    void DestroyEntityDeferred(EntityID entity);

    /**
     * @brief 予約された破棄を実行する。
     */
    void FlushGarbageCollection();

    // --- Component API ---

    /**
     * @brief コンポーネント配列を登録する。
     * @tparam T コンポーネント型
     * @param maxComponents 同時所持可能な最大数
     */
    template <typename T>
    void RegisterComponent(uint32_t maxComponents)
    {
        std::type_index typeName = std::type_index(typeid(T));
        assert(componentArrays_.find(typeName) == componentArrays_.end() && "Component already registered.");

        componentArrays_.insert({ typeName, std::make_unique<ComponentArray<T>>(maxEntities_, maxComponents) });
    }

    /**
     * @brief Entityにコンポーネントを追加する。
     * @tparam T コンポーネント型
     * @param entity 対象Entity
     * @param component データ
     * @param policy 枯渇時のポリシー
     * @return 成功したか
     */
    template <typename T>
    bool AddComponent(EntityID entity, T component, PoolExhaustionPolicy policy = PoolExhaustionPolicy::AssertAndCrash)
    {
        assert(IsAlive(entity) && "Cannot add component to a dead entity.");
        auto array = GetComponentArrayInternal<T>();
        assert(array && "Component not registered.");
        return array->Insert(entity, std::move(component), policy);
    }

    /**
     * @brief Entityからコンポーネントを削除する（即時）。
     * @tparam T コンポーネント型
     * @param entity 対象Entity
     */
    template <typename T>
    void RemoveComponent(EntityID entity)
    {
        if (IsAlive(entity))
        {
            auto array = GetComponentArrayInternal<T>();
            assert(array && "Component not registered.");
            array->Remove(entity);
        }
    }

    /**
     * @brief コンポーネントの参照を取得する。
     * @tparam T コンポーネント型
     * @param entity 対象Entity
     * @return コンポーネントへの参照
     */
    template <typename T>
    T& GetComponent(EntityID entity)
    {
        assert(IsAlive(entity) && "Cannot get component from a dead entity.");
        auto array = GetComponentArrayInternal<T>();
        assert(array && "Component not registered.");
        return array->GetData(entity);
    }

    /**
     * @brief コンポーネントを持っているか。
     * @tparam T コンポーネント型
     * @param entity 対象Entity
     */
    template <typename T>
    bool HasComponent(EntityID entity) const
    {
        if (!IsAlive(entity))
        {
            return false;
        }
        auto array = GetComponentArrayInternal<T>();
        if (!array) return false;
        return array->HasComponent(entity);
    }

    // --- System Integration ---

    /**
     * @brief コンポーネント配列を直接取得する。
     * @tparam T コンポーネント型
     * @return ComponentArrayへの参照
     */
    template <typename T>
    ComponentArray<T>& GetArray()
    {
        std::type_index typeName = std::type_index(typeid(T));
        assert(componentArrays_.find(typeName) != componentArrays_.end() && "Component not registered.");
        return *static_cast<ComponentArray<T>*>(componentArrays_[typeName].get());
    }

    /**
     * @brief コンポーネント型が登録済みか。
     */
    template <typename T>
    bool HasComponentArray() const
    {
        return componentArrays_.find(std::type_index(typeid(T))) != componentArrays_.end();
    }

    /**
     * @brief コンポーネント配列を走査するためのポインタを取得する。
     * @tparam T コンポーネント型
     * @return ComponentArrayへのポインタ（非所有）
     */
    template <typename T>
    ComponentArray<T>* View()
    {
        std::type_index typeName = std::type_index(typeid(T));
        assert(componentArrays_.find(typeName) != componentArrays_.end() && "Component not registered.");
        return static_cast<ComponentArray<T>*>(componentArrays_[typeName].get());
    }

    // --- Debug & Metrics ---

    /**
     * @brief 生存中のEntity数を取得。
     */
    uint32_t GetActiveEntityCount() const { return activeEntityCount_; }

    /**
     * @brief 最大Entityキャパシティを取得。
     */
    uint32_t GetMaxEntityCount() const { return maxEntities_; }

private:
    /**
     * @brief 内部的なコンポーネント配列取得。
     */
    template <typename T>
    ComponentArray<T>* GetComponentArrayInternal() const
    {
        std::type_index typeName = std::type_index(typeid(T));
        auto it = componentArrays_.find(typeName);
        if (it == componentArrays_.end())
        {
            return nullptr;
        }
        return static_cast<ComponentArray<T>*>(it->second.get());
    }

private:
    // 最大Entity数
    uint32_t maxEntities_ = 0;
    // 生存中のEntity数
    uint32_t activeEntityCount_ = 0;

    // 世代管理用 (index -> generation)
    std::vector<uint32_t> generations_;
    // リサイクル可能なIndexキュー
    std::queue<uint32_t> availableIndices_;

    // コンポーネント配列の保持 (所有権あり)
    std::unordered_map<std::type_index, std::unique_ptr<IComponentArray>> componentArrays_;

    // 破棄予約キュー
    std::vector<EntityID> destroyQueue_;
};

// --- Entity Inline Implementations ---

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
    if (IsValid())
    {
        registry_->RemoveComponent<T>(id_);
    }
}

inline void Entity::Destroy()
{
    if (IsValid())
    {
        registry_->DestroyEntityDeferred(id_);
    }
}

inline bool Entity::IsValid() const
{
    return registry_ != nullptr && registry_->IsAlive(id_);
}

