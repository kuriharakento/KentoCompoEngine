#include "math/Vector3.h"

/**
 * @brief プレイヤー固有のステータスやリソースを管理するコンポーネント。
 */
struct PlayerComponent
{
    // スコアや経験値など（インクリメンタルゲーム向け）
    uint64_t score_ = 0;
    uint32_t level_ = 1;
    float exp_ = 0.0f;
    float nextLevelExp_ = 100.0f;
    
    // 無敵フラグ・時間
    bool isInvincible_ = false;
    float invincibleTimer_ = 0.0f;
    
    // バフ・マルチプライヤー
    float attackMultiplier_ = 1.0f;
    float moveSpeedMultiplier_ = 1.0f;

    // --- [BNS-Fix] 移動・回転・回避パラメータ ---
    float rotationSpeed_ = 0.1f;
    
    bool isDodging_ = false;
    float dodgeTimer_ = 0.0f;
    float dodgeCooldownTimer_ = 0.0f;
    Vector3 dodgeDirection_ = { 0, 0, 0 };
    Vector3 dodgeStartPosition_ = { 0, 0, 0 };
    Vector3 dodgeTargetPosition_ = { 0, 0, 0 };

    // 固定値（MoveComponentのデフォルト）
    float dodgeDistance_ = 8.0f;
    float dodgeDuration_ = 0.25f;
    float dodgeCooldown_ = 0.8f;
    float dodgeInvincibleTime_ = 0.25f;

    bool hasMovementInput_ = false;
};
