#pragma once
#include "application/effect/DodgeEffectParticle.h"
#include "application/GameObject/component/base/IGameObjectComponent.h"
#include "math/Vector3.h"
#include "input/Input.h"

class GameObject;

class MoveComponent : public IGameObjectComponent
{
public:
    MoveComponent();
    void Update(GameObject* owner) override;

    // 移動パラメータ設定
    void SetMoveSpeed(float speed) { moveSpeed_ = speed; }

    // 回避パラメータ設定
    void SetDodgeSpeed(float speed) { dodgeSpeed_ = speed; }
    void SetDodgeDuration(float duration) { dodgeDuration_ = duration; }
    void SetDodgeCooldown(float cooldown) { dodgeCooldown_ = cooldown; }
    void SetDodgeInvincibleTime(float time) { dodgeInvincibleTime_ = time; }
    void SetDodgeDistance(float distance) { dodgeDistance_ = distance; }
    void SetRotationSpeed(float speed) { rotationSpeed_ = speed; }

    // 状態取得
    bool IsDodging() const { return dodgeTimer_ > 0.0f; }
    bool IsInvincible() const { return invincibleTimer_ > 0.0f; }
    float GetDodgeProgress() const;  // 回避動作の進行度（0.0～1.0）

private:
    void ProcessMovement(GameObject* owner);
    void ProcessDodge(GameObject* owner);
    Vector3 GetMovementDirection() const;
    void PlayDodgeEffect(GameObject* owner);
    void UpdateRotation(GameObject* owner, const Vector3& direction);  // 向き補間処理

private:
    // 基本移動
    float moveSpeed_ = 9.0f;

    // 回転補間
    float rotationSpeed_ = 0.1f;  // 回転補間速度

    // 回避関連（強化版）
    float dodgeSpeed_ = 30.0f;              // 回避中の移動速度（大幅に上昇）
    float dodgeDuration_ = 0.25f;           // 回避動作の持続時間（短めに）
    float dodgeCooldown_ = 0.8f;            // 回避のクールダウン時間
    float dodgeInvincibleTime_ = 0.25f;     // 回避中の無敵時間
    float dodgeDistance_ = 8.0f;            // 回避距離（目標距離）
    float dodgeImpulse_ = 1.5f;             // 回避開始時の初速倍率
    float dodgeTimer_ = 0.0f;               // 回避タイマー
    float dodgeCooldownTimer_ = 0.0f;       // 回避クールダウンタイマー
    float invincibleTimer_ = 0.0f;          // 無敵タイマー
    Vector3 dodgeDirection_;                // 回避方向
    Vector3 dodgeStartPosition_;            // 回避開始位置
    Vector3 dodgeTargetPosition_;           // 回避目標位置
    bool hasMovementInput_ = false;         // 移動入力があるか
    bool isDodging_ = false;                // 回避中か

    // エフェクト関連
    float effectTimer_ = 0.0f;              // エフェクトタイマー
    float effectInterval_ = 0.03f;          // 残像間隔

    std::unique_ptr<DodgeEffectParticle> dodgeEffect_;
    bool isFirstDodgeFrame_ = false;        // 回避の最初のフレームか
    bool wasEffectPlayed_ = false;          // エフェクト再生済みか
};