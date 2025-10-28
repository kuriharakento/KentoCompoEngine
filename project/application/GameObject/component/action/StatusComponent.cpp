#include "StatusComponent.h"

#include "application/GameObject/base/GameObject.h"
#include "imgui/imgui.h"
#include "time/TimeManager.h"

void StatusComponent::Update(GameObject* owner)
{
#ifdef _DEBUG
	ImGui::Begin("Status Component");
	std::string headerTitle = "Status: " + owner->GetTag();
	ImGui::SeparatorText(headerTitle.c_str());
	ImGui::Text("HP: %.2f / %.2f", hp.GetValue(), maxHp.GetValue());
	ImGui::Text("Attack Power: %.2f", attackPower.GetValue());
	ImGui::Text("Move Speed: %.2f", moveSpeed.GetValue());
	ImGui::End();
#endif

    float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;
    hp.Update(deltaTime);
    maxHp.Update(deltaTime);
    attackPower.Update(deltaTime);
	if (hp.GetValue() <= 0.0f)
	{
		isAlive = false;
	}
}
