#include "CarnageMode.h"
#include "application/combo/ComboManager.h"
#include "application/GameObject/Combatable/character/player/Player.h"
#include "imgui/imgui.h"
#include "time/TimeManager.h"

CarnageMode::CarnageMode(Player* player)
    : player_(player)
{
	effect_ = std::make_unique<CarnageModeEffect>();
	effect_->Initialize();
    timer_ = std::make_unique<Timer>("CarnageTimer", initialTime_);
    timer_->SetOnFinish([this]() {
        RemoveBuffs();
        HideUI();
		effect_->PlayEndEffect(player_->GetPosition());
                        });
}

void CarnageMode::TryStart()
{
    int comboCount = ComboManager::GetInstance().GetComboCount();
    if (!IsActive() && comboCount >= comboThreshold_)
    {
        timer_->Reset();
        timer_->Start();
        ApplyBuffs();
        ShowUI();
		effect_->PlayAuraEffect(player_->GetPosition());
    }
}

void CarnageMode::Update()
{
    ImGui();
    timer_->Update(TimeManager::GetInstance().GetGameContext().realDeltaTime);

    TryStart();

    if (IsActive() && ComboManager::GetInstance().GetComboCount() <= 0)
    {
        timer_->Stop();
        RemoveBuffs();
        HideUI();
    }
}

void CarnageMode::ExtendTimer()
{
    if (IsActive())
    {
        float remain = timer_->GetRemainingTime();
        float duration = remain + extensionTime_;
        timer_->Reset();
        timer_->SetDuration(duration);
        timer_->Start();
    }
}

bool CarnageMode::IsActive() const
{
    return timer_->IsRunning();
}

float CarnageMode::GetTimeLeft() const
{
    return timer_->GetRemainingTime();
}

void CarnageMode::ApplyBuffs()
{
	auto status = player_->GetComponent<StatusComponent>();
    if (!status) return;

    // 攻撃力上昇
    status->attackPower.AddBuff(BuffConfig("CarnageAttackUp", attackUpRate_, BuffType::Percentage));
	// 移動速度上昇
	status->moveSpeed.AddBuff(BuffConfig("CarnageSpeedUp", speedUpRate_, BuffType::Percentage));
}

void CarnageMode::RemoveBuffs()
{
	auto status = player_->GetComponent<StatusComponent>();
    if (!status) return;

    status->attackPower.RemoveBuff("CarnageAttackUp");
	status->moveSpeed.RemoveBuff("CarnageSpeedUp");
}

void CarnageMode::ShowUI()
{
    // UI表示
}

void CarnageMode::HideUI()
{
    // UI非表示
}

void CarnageMode::ImGui()
{
#ifdef USE_IMGUI
    ImGui::Begin("CarnageMode");
    ImGui::Text("Is Active: %s", IsActive() ? "Active" : "No Active");
    ImGui::Text("Time Left: %.2f", GetTimeLeft());
    ImGui::SliderFloat("Attack Up Rate", &attackUpRate_, 0.0f, 2.0f, "%.2f");
    if (ImGui::Button("Extend Timer"))
    {
        ExtendTimer();
    }

	// プレイヤーのステータス表示
    auto status = player_->GetComponent<StatusComponent>();
    if (status)
    {
	    // hp
        if (ImGui::CollapsingHeader("palyer status"))
        {
            ImGui::Text("HP: %.2f / %.2f", status->hp.GetValue(), status->maxHp.GetValue());
            ImGui::Text("Attack Power: %.2f", status->attackPower.GetValue());
        }
    }
    ImGui::End();
#endif
}