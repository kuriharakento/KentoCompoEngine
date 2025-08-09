#include "MoveComponent.h"
#include "application/GameObject/base/GameObject.h"
#include "math/MathUtils.h"
#include <cmath>

#include "math/Easing.h"
#include "time/TimeManager.h"

MoveComponent::MoveComponent()
{
    // 回避エフェクトの初期化
	dodgeEffect_ = std::make_unique<DodgeEffectParticle>();
	dodgeEffect_->Initialize();
}

void MoveComponent::Update(GameObject* owner)
{
    // タイマー更新
    float deltaTime = TimeManager::GetInstance().GetDeltaTime();

    // クールダウンタイマー更新
    if (dodgeCooldownTimer_ > 0.0f)
    {
        dodgeCooldownTimer_ -= deltaTime;
    }

    // 回避タイマー更新
    if (dodgeTimer_ > 0.0f)
    {
        // 前回の位置を保存
        Vector3 prevPosition = owner->GetPosition();

        // 回避タイマー更新
        float previousTime = dodgeTimer_ / dodgeDuration_;
        dodgeTimer_ -= deltaTime;
        float currentTime = (std::max)(0.0f, dodgeTimer_) / dodgeDuration_;

        // 回避処理
        if (isDodging_)
        {
            // イージングを使用して滑らかな回避動作
            float progress = 1.0f - currentTime;  // 0.0～1.0の進行度
            Vector3 newPosition = MathUtils::Lerp(dodgeStartPosition_, dodgeTargetPosition_, progress);
            owner->SetPosition(newPosition);

            // 回避中も向きを滑らかに補間
            UpdateRotation(owner, dodgeDirection_);

            // 回避エフェクト表示
            effectTimer_ -= deltaTime;
            if (effectTimer_ <= 0.0f)
            {
                PlayDodgeEffect(owner);
                effectTimer_ = effectInterval_;
            }
        }

        // 回避終了
        if (dodgeTimer_ <= 0.0f)
        {
            // 回避終了時のエフェクト
            dodgeEffect_->PlayFadeOutEffect(owner->GetPosition());

            dodgeTimer_ = 0.0f;
            isDodging_ = false;
            wasEffectPlayed_ = false; // リセット
            dodgeCooldownTimer_ = dodgeCooldown_;
        }
    }

    // 無敵タイマー更新
    if (invincibleTimer_ > 0.0f)
    {
        invincibleTimer_ -= deltaTime;
        if (invincibleTimer_ < 0.0f)
        {
            invincibleTimer_ = 0.0f;
        }
    }

    // 入力処理（回避中は処理しない）
    if (!isDodging_)
    {
        ProcessDodge(owner);  // 回避を最優先
        if (!isDodging_)
        {    // 回避が始まらなかったら他の処理
            ProcessMovement(owner); // 通常移動
        }
    }
}

// 向きを滑らかに補間する処理
void MoveComponent::UpdateRotation(GameObject* owner, const Vector3& direction)
{
    // 移動方向がある場合のみ向きを更新
    if (direction.Length() > 0.01f)
    {
        // 正規化された方向ベクトル
        Vector3 normalizedDir = direction;
        normalizedDir.Normalize();

        // Y軸回りの目標回転角度を計算（atan2を使用）
        float targetRotationY = atan2f(normalizedDir.x, normalizedDir.z);

        // 現在の回転を取得
        Vector3 currentRotation = owner->GetRotation();

        // Y軸の回転のみ、最短経路で補間
        float easedRotationY = MathUtils::LerpAngle(
            currentRotation.y,
            targetRotationY,
            0.2f // 補間速度（回避中は少し速めに）
        );

        // 回転を更新
        owner->SetRotation({ currentRotation.x, easedRotationY, currentRotation.z });
    }
}

float MoveComponent::GetDodgeProgress() const
{
    if (!IsDodging()) return 0.0f;
    return 1.0f - (dodgeTimer_ / dodgeDuration_);
}

void MoveComponent::ProcessMovement(GameObject* owner)
{
    // 回避中は通常移動しない
    if (IsDodging()) return;

    // 移動方向の取得
    Vector3 moveDirection = GetMovementDirection();
    hasMovementInput_ = moveDirection.Length() > 0.01f;

    // 移動処理
    if (hasMovementInput_)
    {
        moveDirection.Normalize();
        owner->SetPosition(owner->GetPosition() + moveDirection * moveSpeed_ * TimeManager::GetInstance().GetDeltaTime());

        // プレイヤーの向きを滑らかに変える
        UpdateRotation(owner, moveDirection);
    }
}

void MoveComponent::ProcessDodge(GameObject* owner)
{
    // すでに回避中なら処理しない
	if (isDodging_) return;

    // スペースキーで回避
    if (Input::GetInstance()->TriggerKey(DIK_SPACE) && dodgeCooldownTimer_ <= 0.0f)
    {
        // 回避方向の決定（移動方向優先、なければ向いている方向）
        Vector3 moveDirection = GetMovementDirection();

        if (moveDirection.Length() > 0.01f)
        {
            // 移動入力がある場合はその方向に回避
            moveDirection.Normalize();
            dodgeDirection_ = moveDirection;
        }
        else
        {
            // 移動入力がない場合は前方向に回避
            Vector3 rotation = owner->GetRotation();
            float angleY = rotation.y;
            dodgeDirection_ = Vector3(sin(angleY), 0, cos(angleY));
        }

        // 回避開始位置と目標位置を計算
        dodgeStartPosition_ = owner->GetPosition();
        dodgeTargetPosition_ = dodgeStartPosition_ + dodgeDirection_ * dodgeDistance_;

        // 回避開始
        isDodging_ = true;
        isFirstDodgeFrame_ = true;    // 最初のフレームを示すフラグ
        wasEffectPlayed_ = false;     // エフェクト未再生
        dodgeTimer_ = dodgeDuration_;
        invincibleTimer_ = dodgeInvincibleTime_;
        effectTimer_ = 0.0f; // 即座にエフェクト開始

        // 回避開始エフェクト
        PlayDodgeEffect(owner);
    }
    else
    {
        isFirstDodgeFrame_ = false;  // 回避開始フレームでない
    }
}

void MoveComponent::PlayDodgeEffect(GameObject* owner)
{
    // 回避の始まりで一度だけ実行されるエフェクト
    if (isFirstDodgeFrame_ && !wasEffectPlayed_)
    {
        dodgeEffect_->PlayEffect(owner->GetPosition(), dodgeDirection_);
        wasEffectPlayed_ = true;
    }

    // 残像エフェクトを生成
    dodgeEffect_->CreateAfterImage(owner->GetPosition(), owner->GetRotation());
}

Vector3 MoveComponent::GetMovementDirection() const
{
    Vector3 direction(0, 0, 0);

    // WASDキーの入力を取得
    if (Input::GetInstance()->PushKey(DIK_W)) direction.z += 1.0f;
    if (Input::GetInstance()->PushKey(DIK_S)) direction.z -= 1.0f;
    if (Input::GetInstance()->PushKey(DIK_D)) direction.x += 1.0f;
    if (Input::GetInstance()->PushKey(DIK_A)) direction.x -= 1.0f;

    // 長さが0でなければ正規化
    if (direction.Length() > 0.01f)
    {
        direction.Normalize();
    }

    return direction;
}