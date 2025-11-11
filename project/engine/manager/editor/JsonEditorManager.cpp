#include "JsonEditorManager.h"

#include "imgui/imgui.h"

JsonEditorManager* JsonEditorManager::instance_ = nullptr; // シングルトンインスタンス

JsonEditorManager* JsonEditorManager::GetInstance()
{
	if (instance_ == nullptr)
	{
		instance_ = new JsonEditorManager();
	}
	return instance_;
}

void JsonEditorManager::Initialize()
{
	editors_.clear();
}

void JsonEditorManager::Finalize()
{
	editors_.clear();
	delete instance_;
	instance_ = nullptr;
}

void JsonEditorManager::Register(const std::string& name, std::shared_ptr<JsonEditableBase> editor)
{
	editors_[name] = editor;
	selectedItem_ = name; // 最後に登録したものを選択状態にする
}

void JsonEditorManager::RenderEditUI()
{
#ifdef USE_IMGUI
    ImGui::Begin("JSON Editor");

    if (ImGui::BeginTabBar("EditableTabs"))
    {
        for (const auto& [name, editable] : editors_)
        {
            //NULLチェック
            if (!editable) { continue; }

            if (ImGui::BeginTabItem(name.c_str()))
            {
                // タブがアクティブな間は選択状態にしておく
                selectedItem_ = name;

                ImGui::PushID(editable.get());

				editable->DrawOptions(); // オプションを表示

                // そのオブジェクトの ImGui UI を表示
                editable->DrawImGui();

                ImGui::PopID();

                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
#endif
}

void JsonEditorManager::SaveAll()
{

}
