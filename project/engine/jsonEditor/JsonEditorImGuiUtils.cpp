#include "JsonEditorImGuiUtils.h"

// --- float ---
void DrawImGuiForFloat(const std::string& name, float* value)
{
	ImGui::DragFloat(name.c_str(), value, 0.1f);
}

// --- int ---
void DrawImGuiForInt(const std::string& name, int* value)
{
	ImGui::DragInt(name.c_str(), value);
}

// --- bool ---
void DrawImGuiForBool(const std::string& name, bool* value)
{
	ImGui::Checkbox(name.c_str(), value);
}

// --- Vector3 ---
void DrawImGuiForVector3(const std::string& name, Vector3* value)
{
	ImGui::DragFloat3(name.c_str(), &value->x, 0.1f);
}

// --- Transform ---
void DrawImGuiForTransform(const std::string& name, Transform* value)
{
	ImGui::Text("%s", name.c_str());
	ImGui::DragFloat3("Translate", &value->translate.x, 0.1f);
	ImGui::DragFloat3("Rotate", &value->rotate.x, 0.01f);
	ImGui::DragFloat3("Scale", &value->scale.x, 0.1f);
}

// --- std::vector<Vector3> ---
void DrawImGuiForVector3Vector(const std::string& name, std::vector<Vector3>* value)
{
	ImGui::Text("%s", name.c_str());
	for (size_t i = 0; i < value->size(); ++i)
	{
		std::string label = "Element[" + std::to_string(i) + "]";
		ImGui::PushID(static_cast<int>(i));
		DrawImGuiForVector3(label, &(*value)[i]);
		ImGui::PopID();
	}
	ImGui::Separator();
	if (ImGui::Button("Add Vector3"))
	{
		value->push_back(Vector3{ 0.0f, 0.0f, 0.0f });
	}
	ImGui::SameLine();
	if (ImGui::Button("Remove Last") && !value->empty())
	{
		value->pop_back();
	}
}

// --- std::vector<Transform> ---
void DrawImGuiForTransformVector(const std::string& name, std::vector<Transform>* value)
{
	ImGui::Text("%s", name.c_str());
	for (size_t i = 0; i < value->size(); ++i)
	{
		std::string headerLabel = "Transform[" + std::to_string(i) + "]";
		ImGui::PushID(static_cast<int>(i));
		if (ImGui::TreeNode(headerLabel.c_str()))
		{
			ImGui::DragFloat3("Translate", &(*value)[i].translate.x, 0.1f);
			ImGui::DragFloat3("Rotate", &(*value)[i].rotate.x, 0.01f);
			ImGui::DragFloat3("Scale", &(*value)[i].scale.x, 0.1f);
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	ImGui::Separator();
	if (ImGui::Button("Add Transform"))
	{
		value->push_back(Transform{ {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f} });
	}
	ImGui::SameLine();
	if (ImGui::Button("Remove Last") && !value->empty())
	{
		value->pop_back();
	}
}

// --- std::string ---
void DrawImGuiForString(const std::string& name, std::string* value)
{
	char buf[256];
	strncpy_s(buf, value->c_str(), sizeof(buf));
	buf[sizeof(buf) - 1] = '\0';
	if (ImGui::InputText(name.c_str(), buf, sizeof(buf)))
	{
		*value = buf;
	}
}

// --- std::vector<std::string> ---
void DrawImGuiForStringVector(const std::string& name, std::vector<std::string>* value)
{
	ImGui::Text("%s", name.c_str());
	for (size_t i = 0; i < value->size(); ++i)
	{
		std::string label = "Element[" + std::to_string(i) + "]";
		char buf[256];
		strncpy_s(buf, (*value)[i].c_str(), sizeof(buf));
		buf[sizeof(buf) - 1] = '\0';
		if (ImGui::InputText(label.c_str(), buf, sizeof(buf)))
		{
			(*value)[i] = buf;
		}
	}
	ImGui::Separator();
	if (ImGui::Button("Add String"))
	{
		value->push_back("");
	}
	ImGui::SameLine();
	if (ImGui::Button("Remove Last") && !value->empty())
	{
		value->pop_back();
	}
}