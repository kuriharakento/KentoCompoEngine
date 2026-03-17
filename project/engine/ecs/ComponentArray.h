#pragma once

#include "Entity.h"
#include <vector>
#include <cassert>
#include <type_traits>
#include <algorithm>

// Deleted kento_compo and ecs namespaces

/**
 * @brief ComponentArray共通のインターフェース。
 *
 * Registryが型を意識せずに破棄（Entity削除時）を呼べるようにする。
 */
class IComponentArray
{
public:
    virtual ~IComponentArray() = default;
    virtual void EntityDestroyed(EntityID entity) = 0;
};

/**
 * @brief プール枯渇時の生成ポリシー
 */
enum class PoolExhaustionPolicy
{
    // 新規追加をキャンセルして無視する（絶対に消えて欲しくないボス等）
    CancelSpawn,
    // 最も古い要素を破棄（上書き）して追加する（弾やパーティクル等）
    OverwriteOldest,
    // 開発中の設計バグ検出用。Assertで落とす
    AssertAndCrash
};

/**
 * @brief 特定のコンポーネントTを隙間なく連続メモリ上に保持するSparse Set実装。
 *
 * [BNS-Standard] 高性能SoAデータストア。
 * - O(1) でのコンポーネントアクセス、追加、削除 (Swap & Pop) を実現する。
 * - キャッシュ局所性を最大化するため、denseArray_ にデータが物理的に詰まっている。
 * - 動적拡張（再確保）によるイテレータ無効化を防ぐため、初期化時に最大要素数を確保する。
 *
 * @tparam T 格納するコンポーネントの型。POD（Plain Old Data）推奨。
 */
template <typename T>
class ComponentArray : public IComponentArray
{
public:
    /**
     * @brief 最大容量を指定して初期化する。これ以降 vector の拡張は発生させない。
     * @param maxEntities エンティティの最大想定数（Indexの最大値）
     * @param maxComponents このコンポーネントを同時に持つことができる最大数
     */
    ComponentArray(uint32_t maxEntities, uint32_t maxComponents)
        : maxComponents_(maxComponents)
        , validCount_(0)
    {
        // [BNS-Optimization] 索引配列は全確保し、物理ページを確定（Pre-touch）させる
        sparseArray_.resize(maxEntities);
        std::fill(sparseArray_.begin(), sparseArray_.end(), kInvalidEntity);

        // [BNS-Optimization] データ配列は予約のみを行い、デフォルトコンストラクタの無駄な呼び出しを避ける
        denseArray_.reserve(maxComponents);
        entityArray_.reserve(maxComponents);
    }

    /**
     * @brief コンポーネントを追加する。枯渇時はポリシーに従う。
     * @param entity 対象のエンティティID
     * @param component 追加するデータ
     * @param policy キャパシティを超えた場合の振る舞い
     * @return 実際に追加（または上書き）されたか
     */
    bool Insert(EntityID entity, const T& component, PoolExhaustionPolicy policy = PoolExhaustionPolicy::AssertAndCrash)
    {
        uint32_t index = GetEntityIndex(entity);
        assert(index < sparseArray_.size() && "Entity index out of range.");

        // すでに持っている場合は上書き
        if (sparseArray_[index] != kInvalidEntity)
        {
            uint32_t denseIndex = sparseArray_[index];
            denseArray_[denseIndex] = component;
            return true;
        }

        // キャパシティオーバーの場合のフェイルセーフ
        if (validCount_ >= maxComponents_)
        {
            switch (policy)
            {
            case PoolExhaustionPolicy::CancelSpawn:
                return false; // 追加せず諦める

            case PoolExhaustionPolicy::OverwriteOldest:
                // 最も古いもの（通常はDenseの先頭=インデックス0）を犠牲にして新しいものを追加する
                Remove(entityArray_[0]);
                break;

            case PoolExhaustionPolicy::AssertAndCrash:
            default:
                assert(false && "Component pool exhausted!");
                return false;
            }
        }

        // --- 追加処理 ---
        uint32_t newDenseIndex = validCount_;

        // reserve しているので push_back で再確保は発生しない
        denseArray_.push_back(component);
        entityArray_.push_back(entity);
        sparseArray_[index] = newDenseIndex;

        validCount_++;
        return true;
    }

    /**
     * @brief コンポーネントを削除する。Swap & Pop で O(1) とメモリ連続性を両立。
     * @param entity 対象のエンティティID
     */
    void Remove(EntityID entity)
    {
        uint32_t index = GetEntityIndex(entity);
        assert(index < sparseArray_.size());

        uint32_t removedDenseIndex = sparseArray_[index];
        if (removedDenseIndex == kInvalidEntity)
        {
            return; // 持っていないなら何もしない
        }

        // Sparse Set削除（Swap & Pop）の魔法
        uint32_t lastDenseIndex = validCount_ - 1;

        if (removedDenseIndex != lastDenseIndex)
        {
            // 末尾の要素を、削除対象の場所にコピー（移動）して上書きする
            denseArray_[removedDenseIndex] = denseArray_[lastDenseIndex];

            // 逆引き配列も同様に末尾のものを上書き
            EntityID lastEntity = entityArray_[lastDenseIndex];
            entityArray_[removedDenseIndex] = lastEntity;

            // 移動してきた末尾要素の持ち主（Entity）の索引（Sparse）を更新
            uint32_t lastEntityIndex = GetEntityIndex(lastEntity);
            sparseArray_[lastEntityIndex] = removedDenseIndex;
        }

        // 削除対象の索引を無効化し、サイズを減らす
        sparseArray_[index] = kInvalidEntity;

        denseArray_.pop_back();
        entityArray_.pop_back();

        validCount_--;
    }

    /**
     * @brief データへの参照を取得する。
     * @param entity 対象のエンティティID
     * @return コンポーネントの参照
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
     * @brief エンティティがこのコンポーネントを持っているか？
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
     * @brief IComponentArray の実装。Entityの破棄依頼が来た際に自動でコンポーネントも消す。
     */
    void EntityDestroyed(EntityID entity) override
    {
        if (HasComponent(entity))
        {
            Remove(entity);
        }
    }

    // --- Data locality アクセス用 ---

    /**
     * @brief 現在格納されている個数を取得
     */
    uint32_t GetSize() const { return validCount_; }

    /**
     * @brief イテレータ開始
     */
    auto begin() { return denseArray_.begin(); }

    /**
     * @brief イテレータ終了
     */
    auto end() { return denseArray_.begin() + validCount_; }

    /**
     * @brief 特定の密度インデックスからEntityを取得
     * @param denseIndex 密度配列上のインデックス
     * @return 対応するEntityID
     */
    EntityID GetEntityFromDenseIndex(uint32_t denseIndex) const
    {
        assert(denseIndex < validCount_);
        return entityArray_[denseIndex];
    }

    /**
     * @brief 密度インデックスからデータを直接取得（SoA高速走査用）
     * @param denseIndex 密度配列上のインデックス
     * @return コンポーネントの参照
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
    // 密配列：実際のデータが隙間なく詰まっている
    std::vector<T> denseArray_;
    // 逆引き：denseArrayの各インデックスの持ち主
    std::vector<EntityID> entityArray_;
    // 疎配列：EntityのIndexをキーとするルックアップテーブル
    std::vector<uint32_t> sparseArray_;

    // このプールの最大容量（リアロケーション防止用）
    uint32_t maxComponents_;
    // 現在有効なデータ数
    uint32_t validCount_;
};
