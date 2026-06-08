#include "JsonEditorManager.h"
#include "DebugUIManager.h"
#include "imgui/imgui.h"

// シングルトンインスタンスの実体
std::unique_ptr<JsonEditorManager> JsonEditorManager::instance_ = nullptr;

JsonEditorManager* JsonEditorManager::GetInstance()
{
	// インスタンスが存在しない場合は生成
	if (instance_ == nullptr)
	{
		instance_.reset(new JsonEditorManager());
	}
	return instance_.get();
}

void JsonEditorManager::Initialize()
{
	// エディタリストを初期化
	editors_.clear();

#ifdef USE_IMGUI
	// JSONエディタをデバッグUIに登録
	DebugUIManager::GetInstance()->RegisterDebugUI(this, "JSON Editor", [this]() {
		// タブバーを開始
		if (ImGui::BeginTabBar("EditableTabs"))
		{
			// 登録されたすべてのエディタをタブとして表示
			for (const auto& [name, editable] : editors_)
			{
				// NULLチェック
				if (!editable) { continue; }

				// タブアイテムを作成
				if (ImGui::BeginTabItem(name.c_str()))
				{
					// タブがアクティブな間は選択状態にしておく
					selectedItem_ = name;

					// IDを設定して他のエディタと区別する
					ImGui::PushID(editable.get());

					// オプションを表示
					editable->DrawOptions();

					// そのオブジェクトの ImGui UI を表示
					editable->DrawImGui();

					// IDをポップ
					ImGui::PopID();

					ImGui::EndTabItem();
				}
			}
			ImGui::EndTabBar();
		}
	}, DebugUIArea::Console);
#endif
}

void JsonEditorManager::Finalize()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->UnregisterDebugUI(this);
	}
#endif
	// エディタリストをクリア
	editors_.clear();
	// シングルトンインスタンスを解放
	instance_.reset();
}

void JsonEditorManager::Register(const std::string& name, std::shared_ptr<JsonEditableBase> editor)
{
	// エディタをマップに登録
	editors_[name] = editor;
	// 最後に登録したものを選択状態にする
	selectedItem_ = name;
}
void JsonEditorManager::RenderEditUI()
{
	// DrawAreaで描画するため、何もしない
}

void JsonEditorManager::SaveAll()
{
	// 将来的にすべてのエディタの保存処理を実装
}
