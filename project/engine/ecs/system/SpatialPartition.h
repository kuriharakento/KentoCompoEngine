#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include "math/Vector3.h"
#include "math/AABB.h"
#include "engine/ecs/Entity.h"

/**
 * @brief 原子的な操作を避けるため、各スレッドが独立してカウントし、
 *        最後に統合するなどの工夫も可能だが、まずはシンプルな2パス実装とする。
 */
class LinearSpatialHash
{
public:
    static constexpr uint32_t kBucketCount = 8192;
    static constexpr uint32_t kMaxEntitiesPerFrame = 10000;
    static constexpr uint32_t kMaxEntries = kMaxEntitiesPerFrame * 8; // 1エンティティあたり平均8セル

    struct alignas(64) BucketHeader
    {
        uint32_t offset = 0;
        uint32_t count = 0;
    };

    LinearSpatialHash(float cellSize) : cellSize_(cellSize)
    {
        entityBuffer_.resize(kMaxEntries);
        tempCounts_.resize(kBucketCount);
    }

    /**
     * @brief 空間ハッシュをリセットする
     */
    void Clear()
    {
        for (uint32_t i = 0; i < kBucketCount; ++i)
        {
            buckets_[i].offset = 0;
            buckets_[i].count = 0;
            tempCounts_[i] = 0;
        }
        currentEntryCount_ = 0;
    }

    /**
     * @brief パス1: 各バケットの密度をカウントする
     */
    void AddCount(const AABB& bounds)
    {
        IterateCells(bounds, [this](uint32_t hash) {
            buckets_[hash].count++;
        });
    }

    /**
     * @brief パス2: オフセットを確定させる
     */
    void BuildOffsets()
    {
        uint32_t offset = 0;
        for (uint32_t i = 0; i < kBucketCount; ++i)
        {
            buckets_[i].offset = offset;
            offset += buckets_[i].count;
            // 密度を再利用するためカウントは一旦0に戻すが、offsetは保持する
            // 実際の追加時に使う用
            tempCounts_[i] = 0; 
        }
        currentEntryCount_ = offset;
        
        // バッファが不足した場合は拡張（計画書のフェイルセーフ）
        if (currentEntryCount_ > entityBuffer_.size())
        {
            entityBuffer_.resize(currentEntryCount_ * 2);
        }
    }

    /**
     * @brief パス3: エンティティを登録する
     */
    void AddEntity(EntityID entity, const AABB& bounds)
    {
        IterateCells(bounds, [this, entity](uint32_t hash) {
            uint32_t writeIdx = buckets_[hash].offset + tempCounts_[hash]++;
            if (writeIdx < entityBuffer_.size())
            {
                entityBuffer_[writeIdx] = entity;
            }
        });
    }

    /**
     * @brief 指定境界ボックスに重なるエンティティを走査する
     */
    template<typename Func>
    void QueryNearby(const AABB& bounds, Func&& func) const
    {
        IterateCells(bounds, [this, &func](uint32_t hash) {
            const auto& bucket = buckets_[hash];
            for (uint32_t i = 0; i < bucket.count; ++i)
            {
                func(entityBuffer_[bucket.offset + i]);
            }
        });
    }

    /**
     * @brief 指定バケットのオブジェクト密度を取得（ヒートマップ用）
     */
    uint32_t GetBucketCount(uint32_t hash) const { return buckets_[hash].count; }

private:
    /**
     * @brief 3D座標からハッシュ値を生成
     */
    uint32_t GetHash(int64_t x, int64_t y, int64_t z) const
    {
        // 空間ハッシュ関数の定番 (Z-order curve風またはビット混同)
        const int64_t p1 = 73856093;
        const int64_t p2 = 19349663;
        const int64_t p3 = 83492791;
        return static_cast<uint32_t>((x * p1) ^ (y * p2) ^ (z * p3)) % kBucketCount;
    }

    /**
     * @brief AABBが覆うセルに対して関数を実行する
     */
    template<typename Func>
    void IterateCells(const AABB& bounds, Func&& func) const
    {
        int64_t minX = static_cast<int64_t>(std::floor(bounds.min_.x / cellSize_));
        int64_t minY = static_cast<int64_t>(std::floor(bounds.min_.y / cellSize_));
        int64_t minZ = static_cast<int64_t>(std::floor(bounds.min_.z / cellSize_));
        
        int64_t maxX = static_cast<int64_t>(std::floor(bounds.max_.x / cellSize_));
        int64_t maxY = static_cast<int64_t>(std::floor(bounds.max_.y / cellSize_));
        int64_t maxZ = static_cast<int64_t>(std::floor(bounds.max_.z / cellSize_));

        // 安全のためセル範囲を制限（異常なAABBによるフリーズ防止）
        maxX = (std::min)(maxX, minX + 8);
        maxY = (std::min)(maxY, minY + 8);
        maxZ = (std::min)(maxZ, minZ + 8);

        for (int64_t x = minX; x <= maxX; ++x) {
            for (int64_t y = minY; y <= maxY; ++y) {
                for (int64_t z = minZ; z <= maxZ; ++z) {
                    func(GetHash(x, y, z));
                }
            }
        }
    }

    float cellSize_;
    BucketHeader buckets_[kBucketCount];
    std::vector<EntityID> entityBuffer_;
    std::vector<uint32_t> tempCounts_; // Build後の追加用一時カウンタ
    uint32_t currentEntryCount_ = 0;
};


