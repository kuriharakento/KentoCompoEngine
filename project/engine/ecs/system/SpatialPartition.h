#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <unordered_set>
#include "math/Vector3.h"
#include "math/AABB.h"
#include "engine/ecs/Entity.h"

/**
 * @brief 空間分割用のグリッドセル
 */
struct GridCell
{
    std::vector<EntityID> entities;
};

/**
 * @brief 一律グリッド（Uniform Grid）による空間分割の実装。
 * 
 * インクリメンタルゲームのような大量のオブジェクトが動き回る環境で、
 * 衝突判定を O(N^2) から大幅に削減する。
 */
class SpatialGrid
{
public:
    SpatialGrid(float cellSize) : cellSize_(cellSize) {}

    /**
     * @brief エンティティの境界ボックス（AABB）を元にグリッドへ登録する
     */
    void Add(EntityID entity, const AABB& bounds)
    {
        int64_t minX = static_cast<int64_t>(std::floor(bounds.min_.x / cellSize_));
        int64_t minY = static_cast<int64_t>(std::floor(bounds.min_.y / cellSize_));
        int64_t minZ = static_cast<int64_t>(std::floor(bounds.min_.z / cellSize_));
        
        int64_t maxX = static_cast<int64_t>(std::floor(bounds.max_.x / cellSize_));
        int64_t maxY = static_cast<int64_t>(std::floor(bounds.max_.y / cellSize_));
        int64_t maxZ = static_cast<int64_t>(std::floor(bounds.max_.z / cellSize_));

        for (int64_t x = minX; x <= maxX; ++x) {
            for (int64_t y = minY; y <= maxY; ++y) {
                for (int64_t z = minZ; z <= maxZ; ++z) {
                    uint64_t key = GetKey(x, y, z);
                    grid_[key].entities.push_back(entity);
                }
            }
        }
    }

    /**
     * @brief グリッドをクリアする（毎フレーム再構築を想定）
     */
    void Clear()
    {
        grid_.clear();
    }

    /**
     * @brief 指定境界ボックス（AABB）が触れるセルの全エンティティを取得する
     */
    void GetNearbyEntities(const AABB& bounds, std::vector<EntityID>& outEntities) const
    {
        int64_t minX = static_cast<int64_t>(std::floor(bounds.min_.x / cellSize_));
        int64_t minY = static_cast<int64_t>(std::floor(bounds.min_.y / cellSize_));
        int64_t minZ = static_cast<int64_t>(std::floor(bounds.min_.z / cellSize_));
        
        int64_t maxX = static_cast<int64_t>(std::floor(bounds.max_.x / cellSize_));
        int64_t maxY = static_cast<int64_t>(std::floor(bounds.max_.y / cellSize_));
        int64_t maxZ = static_cast<int64_t>(std::floor(bounds.max_.z / cellSize_));

        // 重複を避けるためのセット
        std::unordered_set<EntityID> resultSet;

        for (int64_t x = minX; x <= maxX; ++x) {
            for (int64_t y = minY; y <= maxY; ++y) {
                for (int64_t z = minZ; z <= maxZ; ++z) {
                    uint64_t key = GetKey(x, y, z);
                    auto it = grid_.find(key);
                    if (it != grid_.end()) {
                        for (EntityID entity : it->second.entities) {
                            resultSet.insert(entity);
                        }
                    }
                }
            }
        }
        
        outEntities.assign(resultSet.begin(), resultSet.end());
    }

private:
    // 3D座標をユニークなキーに変換
    uint64_t GetKey(int64_t x, int64_t y, int64_t z) const
    {
        // 簡易的なハッシュ化（グリッドが巨大な場合はオフセット等で調整が必要）
        return (static_cast<uint64_t>(x) & 0x1FFFFF) |
               ((static_cast<uint64_t>(y) & 0x1FFFFF) << 21) |
               ((static_cast<uint64_t>(z) & 0x3FFFFF) << 42);
    }

    float cellSize_;
    std::unordered_map<uint64_t, GridCell> grid_;
};
