#include "MoveComponent.h"
#include "application/GameObject/base/GameObject.h"
#include "math/MathUtils.h"
#include <cmath>

#include "AssaultRifleComponent.h"
#include "KnifeEnemyBehavior.h"
#include "base/Logger.h"
#include "math/Easing.h"
#include "time/TimeManager.h"
#include "application/GameObject/Combatable/character/enemy/EnemyManager.h"
#include "application/GameObject/Combatable/character/player/Player.h"
#include "time/Timer.h"
#include "time/TimerManager.h"

// コンストラクタ：回避エフェクトとマネージャーの初期化
MoveComponent::MoveComponent(EnemyManager* enemyManager, CameraManager* camera)
{
    // 回避エフェクトの初期化
    dodgeEffect_ = std::make_unique<DodgeEffectParticle>();
    dodgeEffect_->Initialize();

    // 敵マネージャーのポインタを保存
    enemyManager_ = enemyManager;
	// カメラのポインタを保存
	camera_ = camera->GetActiveCamera();
}

// フレームごとの更新処理
void MoveComponent::Update(GameObject* owner)
{
    // StatusComponentから移動速度を取得
    auto status = owner->GetComponent<StatusComponent>();
    if (status)
    {
        moveSpeed_ = status->moveSpeed.GetValue();
    }
    else
    {
        // 取得できない場合は処理を中断
        moveSpeed_ = 0.0f;
        Logger::Log("MoveComponent::Update: StatusComponent not found!\n");
        return;
    }

    // デルタタイムを取得（プレイヤーはリアルタイム、それ以外はゲーム時間）
    float deltaTime = 0.0f;
    auto player = dynamic_cast<Player*>(owner);

    if (player)
    {
        deltaTime = TimeManager::GetInstance().GetGameContext().realDeltaTime;
    }
    else
    {
        deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;
    }

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

        // 回避タイマーを更新
        float previousTime = dodgeTimer_ / dodgeDuration_;
        dodgeTimer_ -= deltaTime;
        float currentTime = (std::max)(0.0f, dodgeTimer_) / dodgeDuration_;

        // 回避処理
        if (isDodging_)
        {
            // イージングを使用して滑らかな回避動作
            float progress = 1.0f - currentTime;
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

        // 回避終了処理
        if (dodgeTimer_ <= 0.0f)
        {
            // 回避終了時のエフェクト
            dodgeEffect_->PlayFadeOutEffect(owner->GetPosition());

            // 状態をリセット
            dodgeTimer_ = 0.0f;
            isDodging_ = false;
            wasEffectPlayed_ = false;
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
        // 回避を最優先でチェック
        ProcessDodge(owner);
        if (!isDodging_)
        {
            // 回避が始まらなかったら通常移動
            ProcessMovement(owner, deltaTime);
        }
    }

    // バレットタイム処理
    ProcessBulletTime(owner);
}

// 向きを滑らかに補間する処理
void MoveComponent::UpdateRotation(GameObject* owner, const Vector3& direction)
{
    // 移動方向がある場合のみ向きを更新
    if (direction.Length() > kMovementInputThreshold)
    {
        // 正規化された方向ベクトル
        Vector3 normalizedDir = direction;
        normalizedDir.NormalizeSelf();

        // Y軸回りの目標回転角度を計算
        float targetRotationY = atan2f(normalizedDir.x, normalizedDir.z);

        // 現在の回転を取得
        Vector3 currentRotation = owner->GetRotation();

        // Y軸の回転のみ、最短経路で補間
        float easedRotationY = MathUtils::LerpAngle(
            currentRotation.y,
            targetRotationY,
            kDodgeRotationInterpolation
        );

        // 回転を更新
        owner->SetRotation({ currentRotation.x, easedRotationY, currentRotation.z });
    }
}

// バレットタイム処理
void MoveComponent::ProcessBulletTime(GameObject* owner)
{
    // バレットタイム中または回避中でない場合はスキップ
    if (isInBulletTime_ || !isDodging_) { return; }

    // 敵の攻撃をチェック
    const auto& enemies = enemyManager_->GetEnemies();
    for (const auto& enemy : enemies)
    {
        // 敵の弾との距離をチェック
        auto assaultRifle = enemy->GetComponent<AssaultRifleComponent>();
        if (assaultRifle)
        {
            const auto& bullets = assaultRifle->GetBullets();
            for (auto& bullet : bullets)
            {
                Vector3 toBullet = bullet->GetPosition() - owner->GetPosition();
                float distance = toBullet.Length();

                // バレットタイム範囲内に弾が接近した場合
                if (distance < bulletTimeRadius_)
                {
                    ActivateBulletTime();
                    return;
                }
            }
        }

        // ナイフ敵の近接攻撃をチェック
        auto knifeBehavior = enemy->GetComponent<KnifeEnemyBehavior>();
        if (knifeBehavior && knifeBehavior->IsAttacking())
        {
            Vector3 toEnemy = enemy->GetPosition() - owner->GetPosition();
            float distance = toEnemy.Length();

            // 攻撃範囲内でナイフ敵が攻撃中の場合
            if (distance < bulletTimeRadius_)
            {
                ActivateBulletTime();
                return;
            }
        }
    }

}

// バレットタイム発動
void MoveComponent::ActivateBulletTime()
{
    isInBulletTime_ = true;

    // バレットタイムタイマーを作成
    auto bulletTime = std::make_unique<Timer>("bulletTime", bulletTimeDuration_, DeltaTimeType::RealDeltaTime);
    bulletTime->SetOnStart([this]() {
        // ゲーム時間をスローモーションに
        TimeManager::GetInstance().SetGameTimeScale(bulletTimeScale_);
    });
    bulletTime->SetOnFinish([this]() {
        // ゲーム時間を通常に戻す
        TimeManager::GetInstance().SetGameTimeScale(kNormalTimeScale);

        // クールダウンタイマーを作成
        auto timer = std::make_unique<Timer>("bulletTimeCooldown", bulletTimeCooldown_, DeltaTimeType::RealDeltaTime);
        timer->SetOnFinish([this]() {
            isInBulletTime_ = false;
        });
        TimerManager::GetInstance().AddTimer(std::move(timer));
    });
    TimerManager::GetInstance().AddTimer(std::move(bulletTime));
}

// 回避動作の進行度を取得
float MoveComponent::GetDodgeProgress() const
{
    if (!IsDodging()) return 0.0f;
    return 1.0f - (dodgeTimer_ / dodgeDuration_);
}

// 移動処理
void MoveComponent::ProcessMovement(GameObject* owner, float deltaTime)
{
    // 回避中は通常移動しない
    if (IsDodging()) return;

    // 移動方向を取得
    Vector3 moveDirection = GetMovementDirection();
    hasMovementInput_ = moveDirection.Length() > kMovementInputThreshold;

    // 移動処理
    if (hasMovementInput_)
    {
        moveDirection.NormalizeSelf();
        owner->SetPosition(owner->GetPosition() + moveDirection * moveSpeed_ * deltaTime);

        // プレイヤーの向きを滑らかに変える
        UpdateRotation(owner, moveDirection);
    }
}

// 回避処理
void MoveComponent::ProcessDodge(GameObject* owner)
{
    // すでに回避中なら処理しない
    if (isDodging_) return;

    // スペースキーで回避（クールダウン終了時のみ）
    if (Input::GetInstance()->TriggerKey(DIK_SPACE) && dodgeCooldownTimer_ <= 0.0f)
    {
        // 回避方向の決定（移動方向優先、なければ向いている方向）
        Vector3 moveDirection = GetMovementDirection();

        if (moveDirection.Length() > kMovementInputThreshold)
        {
            // 移動入力がある場合はその方向に回避
            dodgeDirection_ = moveDirection.Normalize();
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
        isFirstDodgeFrame_ = true;
        wasEffectPlayed_ = false;
        dodgeTimer_ = dodgeDuration_;
        invincibleTimer_ = dodgeInvincibleTime_;
        effectTimer_ = 0.0f;

        // 回避開始エフェクト
        PlayDodgeEffect(owner);
    }
    else
    {
        isFirstDodgeFrame_ = false;
    }
}

// 回避エフェクトを再生
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

// 移動方向を取得
Vector3 MoveComponent::GetMovementDirection() const
{
    Vector3 inputDirection(0, 0, 0);

    // WASDキーの入力を取得
    if (Input::GetInstance()->PushKey(DIK_W)) inputDirection.z += 1.0f;
    if (Input::GetInstance()->PushKey(DIK_S)) inputDirection.z -= 1.0f;
    if (Input::GetInstance()->PushKey(DIK_D)) inputDirection.x += 1.0f;
    if (Input::GetInstance()->PushKey(DIK_A)) inputDirection.x -= 1.0f;

    // 入力がない場合は早期リターン
    if (inputDirection.Length() <= kMovementInputThreshold)
    {
        return Vector3(0, 0, 0);
    }

    // カメラが設定されている場合はカメラ基準の方向に変換
    if (camera_ != nullptr)
    {
        return GetCameraRelativeDirection(inputDirection);
    }

    // カメラが設定されていない場合はワールド座標系での移動
    inputDirection.NormalizeSelf();
    return inputDirection;
}

// カメラ基準の方向を取得
Vector3 MoveComponent::GetCameraRelativeDirection(const Vector3& inputDirection) const
{
    if (camera_ == nullptr) return inputDirection;

    // カメラの回転を取得
    Vector3 cameraRotation = camera_->GetRotate();
    float cameraYaw = cameraRotation.y;

    // カメラのY軸回転に基づいて前方向と右方向を計算
    Vector3 cameraForward(std::sin(cameraYaw), 0.0f, std::cos(cameraYaw));
    Vector3 cameraRight(std::cos(cameraYaw), 0.0f, -std::sin(cameraYaw));

    // 入力方向をカメラ基準に変換
    Vector3 moveDirection =
        cameraForward * inputDirection.z +  // 前後入力
        cameraRight * inputDirection.x;     // 左右入力

    // Y軸は常に0に保つ（地面に沿って移動）
    moveDirection.y = 0.0f;

    // 正規化
    if (moveDirection.Length() > kMovementInputThreshold)
    {
        moveDirection.NormalizeSelf();
    }

    return moveDirection;
}