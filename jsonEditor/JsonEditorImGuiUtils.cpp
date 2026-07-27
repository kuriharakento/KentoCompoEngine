#include "JsonEditorImGuiUtils.h"

namespace KCE
{
#ifdef USE_IMGUI

// 文字列入力バッファのサイズ
constexpr int kStringBufferSize = 256;
// 標準のドラッグ速度
constexpr float kDragSpeed = 0.1f;
// 回転用のドラッグ速度（より細かい調整が可能）
constexpr float kRotationDragSpeed = 0.01f;

void DrawImGuiForFloat(const std::string& name, float* value)
{
	ImGui::DragFloat(name.c_str(), value, kDragSpeed);
}

void DrawImGuiForInt(const std::string& name, int* value)
{
	ImGui::DragInt(name.c_str(), value);
}

void DrawImGuiForBool(const std::string& name, bool* value)
{
	ImGui::Checkbox(name.c_str(), value);
}

void DrawImGuiForVector3(const std::string& name, Vector3* value)
{
	ImGui::DragFloat3(name.c_str(), &value->x, kDragSpeed);
}

void DrawImGuiForTransform(const std::string& name, Transform* value)
{
	ImGui::Text("%s", name.c_str());
	// 位置は標準速度
	ImGui::DragFloat3("Translate", &value->translate.x, kDragSpeed);
	// 回転は細かい調整が必要
	ImGui::DragFloat3("Rotate", &value->rotate.x, kRotationDragSpeed);
	// スケールは標準速度
	ImGui::DragFloat3("Scale", &value->scale.x, kDragSpeed);
}

void DrawImGuiForVector3Vector(const std::string& name, std::vector<Vector3>* value)
{
	ImGui::Text("%s", name.c_str());
	// 各要素を描画
	for (size_t i = 0; i < value->size(); ++i)
	{
		std::string label = "Element[" + std::to_string(i) + "]";
		ImGui::PushID(static_cast<int>(i));
		DrawImGuiForVector3(label, &(*value)[i]);
		ImGui::PopID();
	}
	ImGui::Separator();
	// 要素追加ボタン
	if (ImGui::Button("Add Vector3"))
	{
		value->push_back(Vector3{ 0.0f, 0.0f, 0.0f });
	}
	ImGui::SameLine();
	// 要素削除ボタン
	if (ImGui::Button("Remove Last") && !value->empty())
	{
		value->pop_back();
	}
}

void DrawImGuiForTransformVector(const std::string& name, std::vector<Transform>* value)
{
	ImGui::Text("%s", name.c_str());
	// 各要素を描画
	for (size_t i = 0; i < value->size(); ++i)
	{
		std::string headerLabel = "Transform[" + std::to_string(i) + "]";
		ImGui::PushID(static_cast<int>(i));
		if (ImGui::TreeNode(headerLabel.c_str()))
		{
			ImGui::DragFloat3("Translate", &(*value)[i].translate.x, kDragSpeed);
			ImGui::DragFloat3("Rotate", &(*value)[i].rotate.x, kRotationDragSpeed);
			ImGui::DragFloat3("Scale", &(*value)[i].scale.x, kDragSpeed);
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	ImGui::Separator();
	// 要素追加ボタン
	if (ImGui::Button("Add Transform"))
	{
		value->push_back(Transform{ {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f} });
	}
	ImGui::SameLine();
	// 要素削除ボタン
	if (ImGui::Button("Remove Last") && !value->empty())
	{
		value->pop_back();
	}
}

void DrawImGuiForString(const std::string& name, std::string* value)
{
	char buf[kStringBufferSize];
	strncpy_s(buf, value->c_str(), sizeof(buf));
	buf[sizeof(buf) - 1] = '\0';
	if (ImGui::InputText(name.c_str(), buf, sizeof(buf)))
	{
		*value = buf;
	}
}

void DrawImGuiForStringVector(const std::string& name, std::vector<std::string>* value)
{
	ImGui::Text("%s", name.c_str());
	// 各要素を描画
	for (size_t i = 0; i < value->size(); ++i)
	{
		std::string label = "Element[" + std::to_string(i) + "]";
		char buf[kStringBufferSize];
		strncpy_s(buf, (*value)[i].c_str(), sizeof(buf));
		buf[sizeof(buf) - 1] = '\0';
		if (ImGui::InputText(label.c_str(), buf, sizeof(buf)))
		{
			(*value)[i] = buf;
		}
	}
	ImGui::Separator();
	// 要素追加ボタン
	if (ImGui::Button("Add String"))
	{
		value->push_back("");
	}
	ImGui::SameLine();
	// 要素削除ボタン
	if (ImGui::Button("Remove Last") && !value->empty())
	{
		value->pop_back();
	}
}

#endif // USE_IMGUI
} // namespace KCE
