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

inline EntityID MakeEntityID(uint32_t index, uint32_t generation)
{
    return (index & entity_mask::kIndexMask) | ((generation & entity_mask::kGenerationMask) << entity_mask::kIndexBits);
}

class Registry;

/**
 * @brief オブジェクト指向的にECSを操作するためのEntityラッパークラス
 * 
 * 内部的には EntityID(伝票番号) と Registry(倉庫) へのポインタを持つだけであり、
 * メソッド呼び出しをRegistryへの委譲に変換する軽量なラッパーです。
 */
class Entity
{
public:
    Entity() = default;
    Entity(EntityID id, Registry* registry) : id_(id), registry_(registry) {}

    /**
     * @brief コンポーネントを追加する
     * @return 自身への参照（メソッドチェーン可能）
     */
    template<typename T>
    Entity& Add(T component);

    /**
     * @brief コンポーネントを取得する
     * @return コンポーネントへの参照
     */
    template<typename T>
    T& Get();

    /**
     * @brief コンポーネントを所持しているか判定
     */
    template<typename T>
    bool Has() const;

    /**
     * @brief コンポーネントを削除する
     */
    template<typename T>
    void Remove();

    /**
     * @brief このエンティティ自身を破棄（フレーム終端で削除）する
     */
    void Destroy();

    /**
     * @brief 現在このエンティティが有効（生きている）か判定
     */
    bool IsValid() const;

    /**
     * @brief 生のEntityIDを取得
     */
    EntityID GetID() const { return id_; }

    bool operator==(const Entity& other) const { return id_ == other.id_ && registry_ == other.registry_; }
    bool operator!=(const Entity& other) const { return !(*this == other); }
    explicit operator bool() const { return IsValid(); }

private:
    EntityID id_ = kInvalidEntity;
    Registry* registry_ = nullptr;
};
