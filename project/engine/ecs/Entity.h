#pragma once

#include <cstdint>

// Deleted kento_compo and ecs namespaces

/**
 * @brief Entityを一意に識別するID。
 *
 * 内部設計:
 * - uint32_t (32bit) を使用。
 * - 下位 20bit : インデックス (0 〜 1,048,575) ※最大100万個のエンティティ
 * - 上位 12bit : ジェネレーション (世代。インデックスが再利用された回数)
 * 
 * 世代(Generation)を含めることで、古いEntityポインタやIDを保持し続けてしまった場合（ABA問題）に、
 * 現在のRegistry側の世代と比較して「すでに破棄された古いEntityへの不正アクセス」をO(1)で弾くことができる。
 */
using EntityID = uint32_t;

/**
 * @brief 無効なEntityを示す定数。
 */
constexpr EntityID kInvalidEntity = 0xFFFFFFFF;

/**
 * @brief EntityID のビットマスク定義
 */
namespace entity_mask {
    constexpr uint32_t kIndexBits = 20;
    constexpr uint32_t kIndexMask = (1 << kIndexBits) - 1; // 0x000FFFFF

    constexpr uint32_t kGenerationBits = 12;
    constexpr uint32_t kGenerationMask = (1 << kGenerationBits) - 1; // 0x00000FFF
}

/**
 * @brief EntityIDからインデックス部分（下位20bit）を抽出する。
 * @param id 対象のEntityID
 * @return インデックス値
 */
inline uint32_t GetEntityIndex(EntityID id)
{
    return id & entity_mask::kIndexMask;
}

/**
 * @brief EntityIDからジェネレーション部分（上位12bit）を抽出する。
 * @param id 対象のEntityID
 * @return 世代値
 */
inline uint32_t GetEntityGeneration(EntityID id)
{
    return (id >> entity_mask::kIndexBits) & entity_mask::kGenerationMask;
}

/**
 * @brief インデックスと世代から新しいEntityIDを組み立てる。
 * @param index インデックス（20bit以内）
 * @param generation 世代（12bit以内）
 * @return 組み立てられたEntityID
 */
inline EntityID MakeEntityID(uint32_t index, uint32_t generation)
{
    return (index & entity_mask::kIndexMask) | ((generation & entity_mask::kGenerationMask) << entity_mask::kIndexBits);
}


