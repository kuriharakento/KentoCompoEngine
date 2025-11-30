#include "PlayerUIComponent.h"

#include "application/GameObject/base/GameObject.h"
#include "application/GameObject/component/action/AssaultRifleComponent.h"
#include "application/GameObject/component/action/StatusComponent.h"

// HP残量の閾値
constexpr float kHpThresholdHigh = 0.5f;
constexpr float kHpThresholdLow = 0.25f;

// 体力バーの色
const Vector4 kHealthColorHigh = { 0.2f, 0.8f, 0.2f, 1.0f };    // 緑
const Vector4 kHealthColorMedium = { 1.0f, 0.8f, 0.0f, 1.0f };  // 黄
const Vector4 kHealthColorLow = { 1.0f, 0.2f, 0.2f, 1.0f };     // 赤

// リロードバーの色
const Vector4 kReloadBarColor = { 1.0f, 0.8f, 0.0f, 1.0f };     // 黄

PlayerUIComponent::PlayerUIComponent(SpriteCommon* spriteCommon)
    : spriteCommon_(spriteCommon)
{
    InitializeUI();
}

void PlayerUIComponent::InitializeUI()
{
    // 体力バー背景
    healthBarBg_ = std::make_unique<GameUI>();
    healthBarBg_->Initialize(spriteCommon_, "./Resources/UI/hp_bar_frame.png");
    healthBarBg_->SetScreenPosition({ kHealthBarPosX, kHealthBarPosY });
    healthBarBg_->SetSize({ kHealthBarWidth, kHealthBarHeight });
    healthBarBg_->SetAnchorPoint({ 0.0f, 0.5f });
    healthBarBg_->SetInteractable(false);

    // 体力バー
    healthBarFill_ = std::make_unique<GameUI>();
    healthBarFill_->Initialize(spriteCommon_, "./Resources/UI/hp_bar_fill.png");
    healthBarFill_->SetScreenPosition({ kHealthBarPosX, kHealthBarPosY });
    healthBarFill_->SetSize({ kHealthBarWidth, kHealthBarHeight });
    healthBarFill_->SetAnchorPoint({ 0.0f, 0.5f });
    healthBarFill_->SetInteractable(false);

    // 弾薬アイコン
    ammoIcon_ = std::make_unique<GameUI>();
    ammoIcon_->Initialize(spriteCommon_, "./Resources/uvChecker.png");
	ammoIcon_->SetSize({ kAmmoIconSize, kAmmoIconSize });
    ammoIcon_->SetScreenPosition({ kAmmoPosX - 100.0f, kAmmoPosY });
    ammoIcon_->SetInteractable(false);

    // 弾薬数表示
    ammoNumber_ = std::make_unique<NumberSprite>();
    ammoNumber_->Initialize(spriteCommon_, "./Resources/numbers.png", { kDigitWidth, kDigitHeight });
    ammoNumber_->SetPosition({ kAmmoPosX, kAmmoPosY });
	ammoNumber_->SetSpacing(kAmmoNumberSpacing);

    // リロードバー背景
    reloadBarBg_ = std::make_unique<GameUI>();
    reloadBarBg_->Initialize(spriteCommon_, "./Resources/uvChecker.png");
    reloadBarBg_->SetScreenPosition({ kReloadPosX, kReloadPosY });
    reloadBarBg_->SetSize({ kReloadBarWidth, kReloadBarHeight });
    reloadBarBg_->SetAnchorPoint({ 0.5f, 0.5f });
    reloadBarBg_->SetInteractable(false);
    reloadBarBg_->SetVisible(false);

    // リロードバー
    reloadBarFill_ = std::make_unique<GameUI>();
    reloadBarFill_->Initialize(spriteCommon_, "./Resources/uvChecker.png");
    reloadBarFill_->SetScreenPosition({ kReloadPosX - kReloadBarWidth * 0.5f, kReloadPosY });
    reloadBarFill_->SetSize({ 0.0f, kReloadBarHeight });
    reloadBarFill_->SetAnchorPoint({ 0.0f, 0.5f });
    reloadBarFill_->SetInteractable(false);
    reloadBarFill_->SetColor(kReloadBarColor);
    reloadBarFill_->SetVisible(false);
}

void PlayerUIComponent::CacheComponents(GameObject* owner)
{
    if (isInitialized_) return;

    rifleComp_ = owner->GetComponent<AssaultRifleComponent>();
    statusComp_ = owner->GetComponent<StatusComponent>();
    isInitialized_ = true;
}

void PlayerUIComponent::Update(GameObject* owner)
{
    CacheComponents(owner);

    UpdateHealthBar();
    UpdateAmmoDisplay();
    UpdateReloadIndicator();

    // 各UI要素の更新
    if (isHealthBarVisible_)
    {
        healthBarBg_->Update();
        healthBarFill_->Update();
    }

    if (isAmmoDisplayVisible_)
    {
        ammoIcon_->Update();
        ammoNumber_->Update();
    }

    if (isReloadIndicatorVisible_)
    {
        reloadBarBg_->Update();
        reloadBarFill_->Update();
    }
}

void PlayerUIComponent::Draw2D()
{
    // 体力バー
    if (isHealthBarVisible_)
    {
        healthBarBg_->Draw();
        healthBarFill_->Draw();
    }

    // 弾薬表示
    if (isAmmoDisplayVisible_)
    {
        ammoIcon_->Draw();
        ammoNumber_->Draw();
    }

    // リロードインジケーター
    if (isReloadIndicatorVisible_)
    {
        reloadBarBg_->Draw();
        reloadBarFill_->Draw();
    }
}

void PlayerUIComponent::UpdateHealthBar()
{
    auto status = statusComp_.lock();
    if (!status) return;

    // HP比率を計算
    float hpRatio = status->hp.GetValue() / status->maxHp.GetValue();
    if (hpRatio < 0.0f) hpRatio = 0.0f;
    if (hpRatio > 1.0f) hpRatio = 1.0f;

    // 体力バーの幅を更新
    healthBarFill_->SetSize({ healthBarMaxWidth_ * hpRatio, kHealthBarHeight });

    // HP残量に応じて色を変更
    if (hpRatio > kHpThresholdHigh)
    {
        healthBarFill_->SetColor(kHealthColorHigh);
    }
    else if (hpRatio > kHpThresholdLow)
    {
        healthBarFill_->SetColor(kHealthColorMedium);
    }
    else
    {
        healthBarFill_->SetColor(kHealthColorLow);
    }
}

void PlayerUIComponent::UpdateAmmoDisplay()
{
    auto rifle = rifleComp_.lock();
    if (!rifle) return;

    // 弾数を更新
    ammoNumber_->SetNumber(rifle->GetCurrentAmmo());
}

void PlayerUIComponent::UpdateReloadIndicator()
{
    auto rifle = rifleComp_.lock();
    if (!rifle) return;

    bool isReloading = rifle->IsReloading();

    // リロード中のみ表示
    reloadBarBg_->SetVisible(isReloading);
    reloadBarFill_->SetVisible(isReloading);

    if (isReloading)
    {
        // リロード進行度を計算
        float progress = rifle->GetReloadProgress();
        reloadBarFill_->SetSize({ kReloadBarWidth * progress, kReloadBarHeight });
    }
}

void PlayerUIComponent::SetAllVisible(bool isVisible)
{
    isHealthBarVisible_ = isVisible;
    isAmmoDisplayVisible_ = isVisible;
    isReloadIndicatorVisible_ = isVisible;
}