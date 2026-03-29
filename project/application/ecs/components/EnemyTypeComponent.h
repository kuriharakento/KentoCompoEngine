#pragma once
#include <cstdint>

/**
 * @brief 敵の種類を定義する列挙型。
 */
enum class EnemyType : uint32_t
{
    Melee = 0,    // 近接型（プレイヤーに近づく）
    Ranged = 1,   // 遠距離型（射程を保って撃つ - 将来用）
};

/**
 * @brief 敵の種別を識別するためのECSコンポーネント。
 */
struct EnemyTypeComponent
{
    EnemyType type = EnemyType::Melee;
};
