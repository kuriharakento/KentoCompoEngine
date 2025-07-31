#include "StageData.h"

StageData::StageData()
{
	Register("objects", &gameObjects);
}

void StageData::DrawImGui()
{
#ifdef _DEBUG
	ImGui::Begin("Stage Data");
	ImGui::Text("Game Objects");
	for (auto& obj : gameObjects)
	{
		ImGui::Text("Type: %s, Name:%s", obj.type.c_str(), obj.name.c_str());
		ImGui::Checkbox("Disabled", &obj.disabled);
		ImGui::DragFloat3("Position", &obj.transform.translate.x, 0.1f);
		ImGui::DragFloat3("Rotation", &obj.transform.rotate.x, 0.1f);
		ImGui::DragFloat3("Scale", &obj.transform.scale.x, 0.1f);
	}
	ImGui::End();
#endif
}
