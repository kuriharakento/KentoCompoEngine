#include "StatusComponent.h"

#include "engine/gameobject/base/GameObject.h"
#include "imgui/imgui.h"
#ifdef USE_IMGUI
#include "manager/editor/DebugUIManager.h"
#endif
#include "time/TimeManager.h"

StatusComponent::StatusComponent()
{
#ifdef USE_IMGUI
	DebugUIManager::GetInstance()->RegisterDebugUI(this, "Status Component", [this]() { this->DrawImGui(); }, DebugUIArea::Inspector);
#endif
}

StatusComponent::~StatusComponent()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance()) {
		DebugUIManager::GetInstance()->UnregisterDebugUI(this);
	}
#endif
}

void StatusComponent::DrawImGui()
{
#ifdef USE_IMGUI
	if (!lastOwner_)
	{
		ImGui::Text("StatusComponent: No active owner cache.");
		return;
	}
	std::string headerTitle = "Status: " + lastOwner_->GetTag();
	ImGui::SeparatorText(headerTitle.c_str());
	ImGui::Text("HP: %.2f / %.2f", hp.GetValue(), maxHp.GetValue());
	ImGui::Text("Attack Power: %.2f", attackPower.GetValue());
	ImGui::Text("Move Speed: %.2f", moveSpeed.GetValue());
#endif
}

// フレームごとの更新処理
void StatusComponent::Update(GameObject* owner)
{
	lastOwner_ = owner;
	// デルタタイムを取得
    float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;

	// 各ステータス値を更新
    hp.Update(deltaTime);
    maxHp.Update(deltaTime);
    attackPower.Update(deltaTime);
	moveSpeed.Update(deltaTime);
	fireRateMultiplier.Update(deltaTime);

	// HPが0以下になったら死亡判定
	if (hp.GetValue() <= kDeathThreshold)
	{
		isAlive = false;
	}
}
