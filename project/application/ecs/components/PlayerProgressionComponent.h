#pragma once
#include <cstdint>

/**
 * @brief プレイヤーの成長状態（スコア、レベル、経験値）を管理するコンポーネント。
 * 
 * - 実績やレベルアップのトリガーとなるデータのみを保持する。
 * - 戦闘力への反映は ProgressionSystem が StatusComponent を更新することで行う。
 */
struct PlayerProgressionComponent
{
    // 累計データ
    uint64_t totalScore_ = 0;
    
    // 現在の成長状態
    uint32_t level_ = 1;
    float currentExp_ = 0.0f;
    float nextLevelExp_ = 5.0f; // 次のレベルに必要な経験値の閾値 (初期値: 5体)
    
    // インクリメンタル倍率（将来的にはここを成長させる）
    float expMultiplier_ = 1.0f;
    float scoreMultiplier_ = 1.0f;
};
