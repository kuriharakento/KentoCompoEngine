#include "ComboManager.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif 
#include "time/TimeManager.h"

void ComboManager::Initialize(SpriteCommon* spriteCommon)
{
	// コンボ数表示用スプライトの初期化
	comboNumberSprite_.Initialize(spriteCommon, "./Resources/numbers.png", { 64.0f, 64.0f });
}

void ComboManager::OnEnemyDefeated(int count)
{
	comboCount_ += count;
	comboTimer_ = kComboTimeout;
}

void ComboManager::Update()
{
	// Imguiの描画
    DrawImGUi();

    comboNumberSprite_.Update();

    if (comboCount_ > 0)
    {
		comboTimer_ -= TimeManager::GetInstance().GetGameContext().deltaTime;
        if (comboTimer_ <= 0.0f)
        {
            comboCount_ = 0;
            comboTimer_ = 0.0f;
        }
    }
}

void ComboManager::Draw()
{
	// コンボがない場合は描画しない
	if (comboCount_ <= 0)
	{
		return;
	}
	// 画面中央の上くらいに描画
	comboNumberSprite_.DrawNumber(comboCount_, Vector2(580.0f, 100.0f), -20.0f);
}

void ComboManager::Reset()
{
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
