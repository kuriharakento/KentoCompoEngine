#pragma once

#include "Entity.h"
#include <vector>
#include <cassert>
#include <type_traits>
#include <algorithm>

/**
 * @brief ComponentArrayの共通インターフェース。
 */
class IComponentArray
{
public:
    virtual ~IComponentArray() = default;
    virtual void EntityDestroyed(EntityID entity) = 0;
};

/**
 * @brief プール枯渇時の挙動。
 */
enum class PoolExhaustionPolicy
{
    // 追加をキャンセルする
    CancelSpawn,
    // 最も古い要素を上書きする
    OverwriteOldest,
    // アサートで停止する
    AssertAndCrash
};

/**
 * @brief 特定のコンポーネントを連続メモリ上に保持する。
 *
 * Sparse Set実装による SoA データストア。
 * - O(1) でのアクセス、追加、削除。
 * - メモリ連続性を維持しキャッシュ局所性を高める。
 *
 * @tparam T コンポーネント型
 */
template <typename T>
class ComponentArray : public IComponentArray
{
public:
    /**
     * @brief 初期化。最大容量を確保する。
     * @param maxEntities エンティティ最大数
     * @param maxComponents コンポーネント最大保持数
     */
    ComponentArray(uint32_t maxEntities, uint32_t maxComponents)
        : maxComponents_(maxComponents)
        , validCount_(0)
    {
        sparseArray_.resize(maxEntities);
        std::fill(sparseArray_.begin(), sparseArray_.end(), kInvalidEntity);

        denseArray_.reserve(maxComponents);
        entityArray_.reserve(maxComponents);
    }

    /**
     * @brief コンポーネントを追加する。
     * @param entity 対象Entity
     * @param component データ
     * @param policy 枯渇時のポリシー
     * @return 追加（または上書き）されたか
     */
    bool Insert(EntityID entity, T component, PoolExhaustionPolicy policy = PoolExhaustionPolicy::AssertAndCrash)
    {
        uint32_t index = GetEntityIndex(entity);
        assert(index < sparseArray_.size() && "Entity index out of range.");

        // 登録済みなら上書き
        if (sparseArray_[index] != kInvalidEntity)
        {
            uint32_t denseIndex = sparseArray_[index];
            denseArray_[denseIndex] = std::move(component);
            return true;
        }

        // キャパシティオーバー時の処理
        if (validCount_ >= maxComponents_)
        {
            switch (policy)
            {
            case PoolExhaustionPolicy::CancelSpawn:
                return false;

            case PoolExhaustionPolicy::OverwriteOldest:
                Remove(entityArray_[0]);
                break;

            case PoolExhaustionPolicy::AssertAndCrash:
            default:
                assert(false && "Component pool exhausted!");
                return false;
            }
        }

        // 追加処理
        uint32_t newDenseIndex = validCount_;
        denseArray_.push_back(std::move(component));
        entityArray_.push_back(entity);
        sparseArray_[index] = newDenseIndex;

        validCount_++;
        return true;
    }

    /**
     * @brief コンポーネントを削除する。
     * @param entity 対象Entity
     */
    void Remove(EntityID entity)
    {
        uint32_t index = GetEntityIndex(entity);
        assert(index < sparseArray_.size());

        uint32_t removedDenseIndex = sparseArray_[index];
        if (removedDenseIndex == kInvalidEntity)
        {
            return;
        }

        // Swap & Pop で削除
        uint32_t lastDenseIndex = validCount_ - 1;

        if (removedDenseIndex != lastDenseIndex)
        {
            // 末尾を削除位置へ移動
            denseArray_[removedDenseIndex] = std::move(denseArray_[lastDenseIndex]);
            EntityID lastEntity = entityArray_[lastDenseIndex];
            entityArray_[removedDenseIndex] = lastEntity;

            // 索引を更新
            uint32_t lastEntityIndex = GetEntityIndex(lastEntity);
            sparseArray_[lastEntityIndex] = removedDenseIndex;
        }

        sparseArray_[index] = kInvalidEntity;
        denseArray_.pop_back();
        entityArray_.pop_back();

        validCount_--;
    }

    /**
     * @brief データを取得する。
     * @param entity 対象Entity
     * @return データの参照
     */
    T& GetData(EntityID entity)
    {
        uint32_t index = GetEntityIndex(entity);
        assert(index < sparseArray_.size());
        uint32_t denseIndex = sparseArray_[index];
        assert(denseIndex != kInvalidEntity && "Entity does not have this component.");

        return denseArray_[denseIndex];
    }

    /**
     * @brief コンポーネントを保持しているか。
     * @param entity 対象Entity
     */
    bool HasComponent(EntityID entity) const
    {
        uint32_t index = GetEntityIndex(entity);
        if (index >= sparseArray_.size())
        {
            return false;
        }
        return sparseArray_[index] != kInvalidEntity;
    }

    /**
     * @brief Entity破棄時のコールバック。
     * @param entity 破棄されたEntity
     */
    void EntityDestroyed(EntityID entity) override
    {
        if (HasComponent(entity))
        {
            Remove(entity);
        }
    }

    // --- Direct Access ---

    /**
     * @brief 現在の有効データ数を取得。
     */
    uint32_t GetSize() const { return validCount_; }

    auto begin() { return denseArray_.begin(); }
    auto end() { return denseArray_.begin() + validCount_; }

    /**
     * @brief 密度インデックスからEntityIDを取得。
     */
    EntityID GetEntityFromDenseIndex(uint32_t denseIndex) const
    {
        assert(denseIndex < validCount_);
        return entityArray_[denseIndex];
    }

    /**
     * @brief 密度インデックスからデータを直接取得。
     */
    T& GetDataFromDenseIndex(uint32_t denseIndex)
    {
        assert(denseIndex < validCount_);
        return denseArray_[denseIndex];
    }

    const T& GetDataFromDenseIndex(uint32_t denseIndex) const
    {
        assert(denseIndex < validCount_);
        return denseArray_[denseIndex];
    }

private:
    // データ本体
    std::vector<T> denseArray_;
    // 逆引き配列
    std::vector<EntityID> entityArray_;
    // 索引配列 (EntityIndex -> DenseIndex)
    std::vector<uint32_t> sparseArray_;

    // 最大容量
    uint32_t maxComponents_;
    // 有効データ数
    uint32_t validCount_;
};

