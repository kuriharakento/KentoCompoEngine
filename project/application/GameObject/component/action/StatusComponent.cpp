#include "StatusComponent.h"

#include "engine/gameobject/base/GameObject.h"
#include "imgui/imgui.h"
#include "time/TimeManager.h"

// フレームごとの更新処理
void StatusComponent::Update(GameObject* owner)
{
	// ImGuiでデバッグ情報を表示
#ifdef USE_IMGUI
	ImGui::Begin("Status Component");
	std::string headerTitle = "Status: " + owner->GetTag();
	ImGui::SeparatorText(headerTitle.c_str());
	ImGui::Text("HP: %.2f / %.2f", hp.GetValue(), maxHp.GetValue());
	ImGui::Text("Attack Power: %.2f", attackPower.GetValue());
	ImGui::Text("Move Speed: %.2f", moveSpeed.GetValue());
	ImGui::End();
#endif

	// デルタタイムを取得
    float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;

	// 各ステータス値を更新
    hp.Update(deltaTime);
    maxHp.Update(deltaTime);
    attackPower.Update(deltaTime);

	// HPが0以下になったら死亡判定
	if (hp.GetValue() <= kDeathThreshold)
	{
		isAlive = false;
	}
}
