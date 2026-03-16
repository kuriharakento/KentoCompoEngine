#pragma once

#include "Entity.h"
#include <vector>
#include <cassert>
#include <type_traits>

// Deleted kento_compo and ecs namespaces

/**
 * @brief ComponentArray共通のインターフェース。
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
 * - O(1) でのコンポーネントアクセス、追加、削除 (Swap & Pop) を実現する。
 * - キャッシュ局所性を最大化するため、denseArray_ にデータが物理的に詰まっている。
 * - 動的拡張（再確保）によるイテレータ無効化を防ぐため、初期化時に最大要素数を確保する。
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
        : m_maxComponents(maxComponents)
        , m_validCount(0)
    {
        // 索引配列（Sparse）はインデックス指定でアクセスするため全確保
        m_sparseArray.resize(maxEntities, kInvalidEntity);

        // データ配列（Dense）と逆引き配列（Entity）は予約のみ
        m_denseArray.resize(maxComponents);
        m_entityArray.resize(maxComponents);
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
        assert(index < m_sparseArray.size() && "Entity index out of range.");

        // すでに持っている場合は上書き
        if (m_sparseArray[index] != kInvalidEntity)
        {
            uint32_t denseIndex = m_sparseArray[index];
            m_denseArray[denseIndex] = component;
            return true;
        }

        // キャパシティオーバーの場合のフェイルセーフ
        if (m_validCount >= m_maxComponents)
        {
            switch (policy)
            {
            case PoolExhaustionPolicy::CancelSpawn:
                return false; // 追加せず諦める

            case PoolExhaustionPolicy::OverwriteOldest:
                // 最も古いもの（通常はDenseの先頭=インデックス0）を犠牲にして新しいものを追加する
                // 実装をシンプルにするため、インデックス0番目の所有者を強制破棄し、その末尾に追加する
                Remove(m_entityArray[0]);
                break;

            case PoolExhaustionPolicy::AssertAndCrash:
            default:
                assert(false && "Component pool exhausted!");
                return false;
            }
        }

        // --- 追加処理 ---
        uint32_t newDenseIndex = m_validCount;
        m_denseArray[newDenseIndex] = component;   // 末尾に実体保存
        m_entityArray[newDenseIndex] = entity;     // 逆引き登録
        m_sparseArray[index] = newDenseIndex;      // 索引登録
        
        m_validCount++;
        return true;
    }

    /**
     * @brief コンポーネントを削除する。Swap & Pop で O(1) とメモリ連続性を両立。
     * @param entity 対象のエンティティID
     */
    void Remove(EntityID entity)
    {
        uint32_t index = GetEntityIndex(entity);
        assert(index < m_sparseArray.size());

        uint32_t removedDenseIndex = m_sparseArray[index];
        if (removedDenseIndex == kInvalidEntity) return; // 持っていないなら何もしない

        // Sparse Set削除（Swap & Pop）の魔法
        uint32_t lastDenseIndex = m_validCount - 1;

        if (removedDenseIndex != lastDenseIndex)
        {
            // 末尾の要素を、削除対象の場所にコピー（移動）して上書きする
            m_denseArray[removedDenseIndex] = m_denseArray[lastDenseIndex];
            
            // 逆引き配列も同様に末尾のものを上書き
            EntityID lastEntity = m_entityArray[lastDenseIndex];
            m_entityArray[removedDenseIndex] = lastEntity;

            // 移動してきた末尾要素の持ち主（Entity）の索引（Sparse）を更新
            uint32_t lastEntityIndex = GetEntityIndex(lastEntity);
            m_sparseArray[lastEntityIndex] = removedDenseIndex;
        }

        // 削除対象の索引を無効化し、サイズを減らす
        m_sparseArray[index] = kInvalidEntity;
        m_validCount--;
    }

    /**
     * @brief データへの参照を取得する。
     * @param entity 対象のエンティティID
     * @return コンポーネントの参照
     */
    T& GetData(EntityID entity)
    {
        uint32_t index = GetEntityIndex(entity);
        assert(index < m_sparseArray.size());
        uint32_t denseIndex = m_sparseArray[index];
        assert(denseIndex != kInvalidEntity && "Entity does not have this component.");
        
        return m_denseArray[denseIndex];
    }

    /**
     * @brief エンティティがこのコンポーネントを持っているか？
     */
    bool HasComponent(EntityID entity) const
    {
        uint32_t index = GetEntityIndex(entity);
        if (index >= m_sparseArray.size()) return false;
        return m_sparseArray[index] != kInvalidEntity;
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
    // これにより、Systemは denseArray_ をイテレータで純粋かつ高速に走査できる。

    uint32_t GetSize() const { return m_validCount; }
    
    // 直アクセスのイテレータ（std::vector のイテレータをそのまま公開）
    // 注意: Swap & Popの性質上、順序は保証されませんが、全結合処理の場合は最速です。
    auto begin() { return m_denseArray.begin(); }
    auto end() { return m_denseArray.begin() + m_validCount; }
    
    // 特定のインデックスから逆引きでEntityを取得する（Systemが「誰のデータか」知るため）
    EntityID GetEntityFromDenseIndex(uint32_t denseIndex) const
    {
        assert(denseIndex < m_validCount);
        return m_entityArray[denseIndex];
    }

private:
    // 密配列：実際のデータが隙間なく詰まっている
    std::vector<T> m_denseArray;
    // 逆引き：denseArrayの各インデックスの持ち主
    std::vector<EntityID> m_entityArray;
    // 疎配列：EntityのIndexをキーとするルックアップテーブル
    std::vector<uint32_t> m_sparseArray;

    // このプールの最大容量（リアロケーション防止用）
    uint32_t m_maxComponents;
    // 現在有効なデータ数
    uint32_t m_validCount;
};


