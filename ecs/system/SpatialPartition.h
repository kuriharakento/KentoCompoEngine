#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include "math/Vector3.h"
#include "math/AABB.h"
#include "engine/ecs/Entity.h"

namespace KCE
{
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

    LinearSpatialHash(float cellSize) : cellSize_(cellSize), cellSizeInv_(1.0f / cellSize)
    {
        entityBuffer_.resize(kMaxEntries);
        tempCounts_.resize(kBucketCount);
    }

    /**
     * @brief 空間ハッシュをリセットする
     */
    void Clear()
    {
        // ゼロ初期化を最優先（memset等と同等の速度を期待）
        std::fill(std::begin(buckets_), std::end(buckets_), BucketHeader{});
        std::fill(tempCounts_.begin(), tempCounts_.end(), 0u);
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
        
        // バッファが不足した場合は拡張
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
            const uint32_t count = bucket.count;
            const uint32_t offset = bucket.offset;
            for (uint32_t i = 0; i < count; ++i)
            {
                func(entityBuffer_[offset + i]);
            }
        });
    }

    /**
     * @brief 指定バケットのオブジェクト密度を取得（ヒートマップ用）
     */
    uint32_t GetBucketCount(uint32_t hash) const { return buckets_[hash].count; }

private:
    /**
     * @brief 3D座標からハッシュ値を生成 (高速版)
     */
    uint32_t GetHash(int64_t x, int64_t y, int64_t z) const
    {
        // 空間ハッシュ関数の定番
        constexpr uint32_t p1 = 73856093u;
        constexpr uint32_t p2 = 19349663u;
        constexpr uint32_t p3 = 83492791u;
        return ((uint32_t)x * p1 ^ (uint32_t)y * p2 ^ (uint32_t)z * p3) % kBucketCount;
    }

    /**
     * @brief AABBが覆うセルに対して関数を実行する
     */
    template<typename Func>
    void IterateCells(const AABB& bounds, Func&& func) const
    {
        const float invS = cellSizeInv_;
        int64_t minX = static_cast<int64_t>(bounds.min_.x * invS);
        int64_t minY = static_cast<int64_t>(bounds.min_.y * invS);
        int64_t minZ = static_cast<int64_t>(bounds.min_.z * invS);
        
        int64_t maxX = static_cast<int64_t>(bounds.max_.x * invS);
        int64_t maxY = static_cast<int64_t>(bounds.max_.y * invS);
        int64_t maxZ = static_cast<int64_t>(bounds.max_.z * invS);

        // 床関数(floor)相当。負の座標対応。
        if (bounds.min_.x < 0) minX--;
        if (bounds.min_.y < 0) minY--;
        if (bounds.min_.z < 0) minZ--;
        if (bounds.max_.x < 0) maxX--;
        if (bounds.max_.y < 0) maxY--;
        if (bounds.max_.z < 0) maxZ--;

        // 走査範囲を制限（巨大なAABBによるストール防止）
        constexpr int64_t kMaxCellRange = 64; // 長大なビームなどを考慮し引き上げ
        maxX = (std::min)(maxX, minX + kMaxCellRange);
        maxY = (std::min)(maxY, minY + kMaxCellRange);
        maxZ = (std::min)(maxZ, minZ + kMaxCellRange);

        for (int64_t x = minX; x <= maxX; ++x) {
            for (int64_t y = minY; y <= maxY; ++y) {
                for (int64_t z = minZ; z <= maxZ; ++z) {
                    func(GetHash(x, y, z));
                }
            }
        }
    }

    float cellSize_;
    float cellSizeInv_;
    BucketHeader buckets_[kBucketCount];
    std::vector<EntityID> entityBuffer_;
    std::vector<uint32_t> tempCounts_;
    uint32_t currentEntryCount_ = 0;
};
} // namespace KCE
