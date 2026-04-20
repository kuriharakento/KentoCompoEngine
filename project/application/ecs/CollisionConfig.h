#pragma once
#include <cstdint>

/**
 * @brief アプリケーション固有の衝突判定レイヤー定義。
 * 
 * エンジンコアはこの定数を知らず、uint32_t のビットマスクとしてのみ扱います。
 * 複数のレイヤーと衝突させたい場合は、OR 演算 (|) を使用してください。
 */
namespace CollisionLayer
{
    static constexpr uint32_t None         = 0;
    static constexpr uint32_t Player       = 1 << 0; // プレイヤー
    static constexpr uint32_t Enemy        = 1 << 1; // 敵
    static constexpr uint32_t Obstacle     = 1 << 2; // 障害物
    static constexpr uint32_t PlayerBullet = 1 << 3; // プレイヤーの弾
    static constexpr uint32_t EnemyBullet  = 1 << 4; // 敵の弾
    static constexpr uint32_t Decoy        = 1 << 5; // デコイ
    
    // プリセットマクロなどの定義もここで行うと便利です
    static constexpr uint32_t All = 0xFFFFFFFF;
}
