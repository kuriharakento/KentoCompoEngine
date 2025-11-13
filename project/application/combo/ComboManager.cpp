#include "ComboManager.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif 
#include "time/TimeManager.h"

void ComboManager::Initialize(SpriteCommon* spriteCommon)
{
	// コンボ数表示用の数字スプライト初期化
	comboNumberSprite_.Initialize(spriteCommon, "./Resources/numbers.png", { 64.0f, 64.0f });
}

void ComboManager::OnEnemyDefeated(int count)
{
	// コンボ数を増加し、タイマーをリセット
	comboCount_ += count;
	comboTimer_ = kComboTimeout;
}

void ComboManager::Update()
{
	// デバッグ情報の表示
    DrawImGUi();

    comboNumberSprite_.Update();

	// コンボがアクティブな場合のみタイマー処理
    if (comboCount_ > 0)
    {
		// タイマーをデクリメント
		comboTimer_ -= TimeManager::GetInstance().GetGameContext().deltaTime;
		
		// タイムアウト時にコンボをリセット
        if (comboTimer_ <= 0.0f)
        {
            comboCount_ = 0;
            comboTimer_ = 0.0f;
        }
    }
}

void ComboManager::Draw()
{
	// コンボがない場合は描画をスキップ（パフォーマンス最適化）
	if (comboCount_ <= 0)
	{
		return;
	}
	
	// 画面中央上部にコンボ数を表示
	comboNumberSprite_.DrawNumber(comboCount_, Vector2(580.0f, 100.0f), -20.0f);
}

void ComboManager::Reset()
{
	// コンボ数とタイマーを初期化
    comboCount_ = 0;
    comboTimer_ = 0.0f;
}

void ComboManager::DrawImGUi()
{
#ifdef USE_IMGUI
	ImGui::Begin("ComboManager");
	ImGui::Text("Combo Count: %d", comboCount_);
	ImGui::Text("Combo Timer: %.2f", comboTimer_);
	ImGui::End();
#endif
}
