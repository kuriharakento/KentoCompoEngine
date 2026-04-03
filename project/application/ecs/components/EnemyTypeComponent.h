#pragma once
#include <cstdint>

/**
 * @brief 敵の種類を定義する列挙型。
 */
enum class EnemyType : uint32_t
{
    Melee = 0,    // 近接型（プレイヤーに近づく）
    Ranged = 1,   // 遠距離型（射程を保って撃つ - 将来用）
    Charger = 2,  // 突進型（プレイヤーに狙いを定めて高速で突っ込む）
};

/**
 * @brief 敵の種別を識別するためのECSコンポーネント。
 */
struct EnemyTypeComponent
{
    EnemyType type = EnemyType::Melee;
};
