#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include "math/Vector3.h"
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
     * @brief エンティティを指定座標のセルに登録する
     */
    void Add(EntityID entity, const Vector3& position)
    {
        int64_t x = static_cast<int64_t>(std::floor(position.x / cellSize_));
        int64_t y = static_cast<int64_t>(std::floor(position.y / cellSize_));
        int64_t z = static_cast<int64_t>(std::floor(position.z / cellSize_));
        
        uint64_t key = GetKey(x, y, z);
        grid_[key].entities.push_back(entity);
    }

    /**
     * @brief グリッドをクリアする（毎フレーム再構築を想定）
     */
    void Clear()
    {
        grid_.clear();
    }

    /**
     * @brief 指定座標の周辺セルのエンティティを取得する
     */
    void GetNearbyEntities(const Vector3& position, std::vector<EntityID>& outEntities) const
    {
        int64_t cx = static_cast<int64_t>(std::floor(position.x / cellSize_));
        int64_t cy = static_cast<int64_t>(std::floor(position.y / cellSize_));
        int64_t cz = static_cast<int64_t>(std::floor(position.z / cellSize_));

        // 3x3x3 セルを走査
        for (int64_t x = cx - 1; x <= cx + 1; ++x) {
            for (int64_t y = cy - 1; y <= cy + 1; ++y) {
                for (int64_t z = cz - 1; z <= cz + 1; ++z) {
                    uint64_t key = GetKey(x, y, z);
                    auto it = grid_.find(key);
                    if (it != grid_.end()) {
                        outEntities.insert(outEntities.end(), it->second.entities.begin(), it->second.entities.end());
                    }
                }
            }
        }
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
