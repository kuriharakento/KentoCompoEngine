#include "CarnageMode.h"

#include "application/combo/ComboManager.h"
#include "application/GameObject/Combatable/character/player/Player.h"

void CarnageMode::Initialize(Player* player)
{

}

void CarnageMode::Update(float deltaTime)
{
    int comboCount = ComboManager::GetInstance().GetComboCount();

    // 発動判定
    if (!isActive_ && comboCount >= comboThreshold_)
    {
        Start();
    }

    // コンボ数が増えた瞬間のみタイマー延長
    if (isActive_ && comboCount > prevComboCount_)
    {
        carnageTimer_ += extensionTime_;
    }

    // 終了判定
    if (isActive_)
    {
        carnageTimer_ -= deltaTime;
        if (comboCount <= 0 || carnageTimer_ <= 0.0f)
        {
            End();
        }
    }

    prevComboCount_ = comboCount; // 毎フレーム保存
}

void CarnageMode::Start()
{
    isActive_ = true;
    carnageTimer_ = initialTime_;
    ApplyBuffs();
    ShowUI();
    // TODO: BGM/エフェクト切り替え
}

void CarnageMode::End()
{
    isActive_ = false;
    RemoveBuffs();
    HideUI();
    // TODO: BGM/エフェクト戻す
}

void CarnageMode::ApplyBuffs()
{
	auto status = player_->GetComponent<StatusComponent>();
    if (!status) return;
    status->attackPower.AddBuff(BuffConfig("CarnageAttackUp", 0.5f, BuffType::Percentage));
    status->hp.AddBuff(BuffConfig("CarnageInvincible", 99999.0f, BuffType::Override));
}

void CarnageMode::RemoveBuffs()
{
	auto status = player_->GetComponent<StatusComponent>();
    if (!status) return;
    status->attackPower.RemoveBuff("CarnageAttackUp");
    status->hp.RemoveBuff("CarnageInvincible");
}

void CarnageMode::ShowUI()
{
    // カーネージモードのUI表示処理
}

void CarnageMode::HideUI()
{
    // UI非表示処理
}