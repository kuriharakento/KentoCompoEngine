#include "CarnageMode.h"
#include "application/combo/ComboManager.h"
#include "application/GameObject/Combatable/character/player/Player.h"
#include "imgui/imgui.h"
#include "time/TimeManager.h"

CarnageMode::CarnageMode(Player* player)
    : player_(player)
{
	// カーネージモード専用エフェクトの初期化
	effect_ = std::make_unique<CarnageModeEffect>();
	effect_->Initialize();
	
	// タイマーの設定（終了時のコールバックを登録）
    timer_ = std::make_unique<Timer>("CarnageTimer", initialTime_);
    timer_->SetOnFinish([this]() {
        RemoveBuffs();      // バフを解除
        HideUI();           // UIを非表示
		effect_->PlayEndEffect(player_->GetPosition()); // 終了エフェクト再生
    });
}

void CarnageMode::TryStart()
{
	// コンボ条件を満たした場合にカーネージモードを発動
    int comboCount = ComboManager::GetInstance().GetComboCount();
    if (!IsActive() && comboCount >= comboThreshold_)
    {
        timer_->Reset();
        timer_->Start();
        ApplyBuffs();  // プレイヤーにバフを適用
        ShowUI();      // カーネージモードUIを表示
		effect_->PlayAuraEffect(player_->GetPosition()); // オーラエフェクト開始
    }
}

void CarnageMode::Update()
{
	// デバッグ情報表示
    ImGui();
    
    // タイマー更新（リアルタイム基準）
    timer_->Update(TimeManager::GetInstance().GetGameContext().realDeltaTime);

	// 毎フレーム発動条件をチェック
    TryStart();

	// コンボが途切れた場合は強制終了
    if (IsActive() && ComboManager::GetInstance().GetComboCount() <= 0)
    {
        timer_->Stop();
        RemoveBuffs();
        HideUI();
    }
}

void CarnageMode::ExtendTimer()
{
	// カーネージモード中に敵を倒した場合、タイマーを延長
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
	// プレイヤーのステータスコンポーネントを取得
	auto status = player_->GetComponent<StatusComponent>();
    if (!status) return;

    // 攻撃力と移動速度にパーセンテージバフを適用
    status->attackPower.AddBuff(BuffConfig("CarnageAttackUp", attackUpRate_, BuffType::Percentage));
	status->moveSpeed.AddBuff(BuffConfig("CarnageSpeedUp", speedUpRate_, BuffType::Percentage));
}

void CarnageMode::RemoveBuffs()
{
	// プレイヤーのステータスコンポーネントを取得
	auto status = player_->GetComponent<StatusComponent>();
    if (!status) return;

	// カーネージモードのバフを削除
    status->attackPower.RemoveBuff("CarnageAttackUp");
	status->moveSpeed.RemoveBuff("CarnageSpeedUp");
}

void CarnageMode::ShowUI()
{
    // カーネージモードUI表示処理（将来実装）
}

void CarnageMode::HideUI()
{
    // カーネージモードUI非表示処理（将来実装）
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