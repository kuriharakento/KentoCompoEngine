#pragma once
#include <cstdint>

/**
 * @brief 敵の種類を定義する列挙型。
 */
enum class EnemyType : uint32_t
{
    Melee = 0,    // 近接型（プレイヤーに近づく）
    Charger = 1,  // 突進型（プレイヤーに狙いを定めて高速で突っ込む）
};

/**
 * @brief 敵の種別を識別するためのECSコンポーネント。
 */
struct EnemyTypeComponent
{
    EnemyType type = EnemyType::Melee;
};
