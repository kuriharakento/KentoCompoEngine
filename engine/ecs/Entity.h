#pragma once

#include <cstdint>

/**
 * @brief Entityを一意に識別するID。
 *
 * 内部設計:
 * - 32bit整数を使用。
 * - 下位 20bit : インデックス (最大約100万個)
 * - 上位 12bit : ジェネレーション (再利用回数)
 * 
 * 世代管理により、古いIDでの不正アクセスをO(1)で検知できる。
 */
using EntityID = uint32_t;

/**
 * @brief 無効なEntityを示す定数。
 */
constexpr EntityID kInvalidEntity = 0xFFFFFFFF;

/**
 * @brief EntityID のビットマスク定義
 */
namespace entity_mask
{
    constexpr uint32_t kIndexBits = 20;
    constexpr uint32_t kIndexMask = (1 << kIndexBits) - 1; // 0x000FFFFF

    constexpr uint32_t kGenerationBits = 12;
    constexpr uint32_t kGenerationMask = (1 << kGenerationBits) - 1; // 0x00000FFF
}

/**
 * @brief IDからインデックス部分を抽出する。
 * @param id EntityID
 * @return インデックス値
 */
inline uint32_t GetEntityIndex(EntityID id)
{
    return id & entity_mask::kIndexMask;
}

/**
 * @brief IDから世代部分を抽出する。
 * @param id EntityID
 * @return 世代値
 */
inline uint32_t GetEntityGeneration(EntityID id)
{
    return (id >> entity_mask::kIndexBits) & entity_mask::kGenerationMask;
}

/**
 * @brief インデックスと世代からIDを合成する。
 * @param index インデックス
 * @param generation 世代
 * @return 合成されたEntityID
 */
inline EntityID MakeEntityID(uint32_t index, uint32_t generation)
{
    return (index & entity_mask::kIndexMask) | ((generation & entity_mask::kGenerationMask) << entity_mask::kIndexBits);
}

class Registry;

/**
 * @brief Entity操作用の軽量ラッパー。
 * 
 * IDとRegistryへの参照を持ち、メソッド呼び出しをRegistryへ委譲する。
 */
class Entity
{
public:
    Entity() = default;
    Entity(EntityID id, Registry* registry) : id_(id), registry_(registry) {}

    /**
     * @brief コンポーネントを追加する。
     * @tparam T コンポーネント型
     * @return 自身への参照
     */
    template<typename T>
    Entity& Add(T component);

    /**
     * @brief コンポーネントを取得する。
     * @tparam T コンポーネント型
     * @return コンポーネントへの参照
     */
    template<typename T>
    T& Get();

    /**
     * @brief コンポーネントを所持しているか。
     * @tparam T コンポーネント型
     */
    template<typename T>
    bool Has() const;

    /**
     * @brief コンポーネントを削除する。
     * @tparam T コンポーネント型
     */
    template<typename T>
    void Remove();

    /**
     * @brief Entityを破棄する（フレーム末尾で実行）。
     */
    void Destroy();

    /**
     * @brief 有効なEntityか。
     */
    bool IsValid() const;

    /**
     * @brief 生のIDを取得する。
     */
    EntityID GetID() const { return id_; }

    bool operator==(const Entity& other) const { return id_ == other.id_ && registry_ == other.registry_; }
    bool operator!=(const Entity& other) const { return !(*this == other); }
    explicit operator bool() const { return IsValid(); }

private:
    // 生のID
    EntityID id_ = kInvalidEntity;
    // 管理元。所有しない
    Registry* registry_ = nullptr;
};

