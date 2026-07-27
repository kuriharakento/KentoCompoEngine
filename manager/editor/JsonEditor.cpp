#include "JsonEditor.h"
#include "DebugUIManager.h"
#include "imgui/imgui.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace KCE
{
// シングルトンインスタンスの実体
std::unique_ptr<JsonEditor> JsonEditor::instance_ = nullptr;

JsonEditor* JsonEditor::GetInstance()
{
	// インスタンスが存在しない場合は生成
	if (instance_ == nullptr)
	{
		instance_ = std::make_unique<JsonEditor>();
	}
	return instance_.get();
}

void JsonEditor::Initialize()
{
	// エディタリストを初期化
	editors_.clear();
	sharedEditors_.clear();

#ifdef USE_IMGUI
	// JSONエディタをデバッグUIに登録（再起動時に消えないように Project エリアに登録する）
	DebugUIManager::GetInstance()->RegisterDebugUI(this, "JSON Editor", [this]() {
		// タブバーを開始
		if (ImGui::BeginTabBar("EditableTabs"))
		{
			// 1. 汎用JSON編集タブ（常に表示され、任意のファイル名を指定・選択してロード・編集・保存できる）
			if (ImGui::BeginTabItem("Raw JSON Editor"))
			{
				ImGui::Spacing();
				ImGui::SeparatorText("Raw JSON File Loader / Editor");

				// ターゲットファイル名
				char rawFileBuf[256];
				strncpy_s(rawFileBuf, rawJsonFileName_.c_str(), sizeof(rawFileBuf));
				rawFileBuf[sizeof(rawFileBuf) - 1] = '\0';
				ImGui::PushItemWidth(250.0f);
				if (ImGui::InputText("JSON File Name", rawFileBuf, sizeof(rawFileBuf)))
				{
					rawJsonFileName_ = rawFileBuf;
				}
				ImGui::PopItemWidth();

				// Resources/json/ のファイル一覧を取得
				std::vector<std::string> jsonFiles;
				std::string dirPath = "Resources/json/";
				try
				{
					if (std::filesystem::exists(dirPath))
					{
						for (const auto& entry : std::filesystem::directory_iterator(dirPath))
						{
							if (entry.is_regular_file() && entry.path().extension() == ".json")
							{
								jsonFiles.push_back(entry.path().filename().string());
							}
						}
					}
				}
				catch (...) {}

				// ドロップダウン表示
				if (!jsonFiles.empty())
				{
					ImGui::SameLine();
					ImGui::PushItemWidth(200.0f);
					if (ImGui::BeginCombo("##RawFileSelectCombo", rawJsonFileName_.c_str()))
					{
						for (const auto& file : jsonFiles)
						{
							bool isSelected = (rawJsonFileName_ == file);
							if (ImGui::Selectable(file.c_str(), isSelected))
							{
								rawJsonFileName_ = file;
								// 選択されたファイルを自動ロード
								std::ifstream ifs(dirPath + rawJsonFileName_);
								if (ifs)
								{
									std::stringstream ss;
									ss << ifs.rdbuf();
									rawJsonContentStr_ = ss.str();
								}
							}
						}
						ImGui::EndCombo();
					}
					ImGui::PopItemWidth();
				}

				ImGui::Spacing();

				// 読込・保存ボタン
				if (ImGui::Button("Load Raw JSON"))
				{
					std::ifstream ifs(dirPath + rawJsonFileName_);
					if (ifs)
					{
						std::stringstream ss;
						ss << ifs.rdbuf();
						rawJsonContentStr_ = ss.str();
					}
					else
					{
						KCE::Logger::Log("[JSON Error] Failed to open raw JSON file: " + dirPath + rawJsonFileName_ + "\n");
						rawJsonContentStr_ = "{}";
					}
				}
				ImGui::SameLine();
				if (ImGui::Button("Save Raw JSON"))
				{
					std::ofstream ofs(dirPath + rawJsonFileName_);
					if (ofs)
					{
						ofs << rawJsonContentStr_;
					}
					else
					{
						KCE::Logger::Log("[JSON Error] Failed to save raw JSON file: " + dirPath + rawJsonFileName_ + "\n");
					}
				}

				ImGui::Spacing();
				ImGui::Text("File Contents (Raw Text Edit):");

				// 編集バッファの準備
				static std::vector<char> textBuf;
				textBuf.assign(rawJsonContentStr_.begin(), rawJsonContentStr_.end());
				textBuf.push_back('\0');
				// バッファを余分に確保
				if (textBuf.size() < 65536)
				{
					textBuf.resize(65536, '\0');
				}

				if (ImGui::InputTextMultiline("##RawTextEditorArea", textBuf.data(), textBuf.size(), ImVec2(-FLT_MIN, 300.0f), ImGuiInputTextFlags_AllowTabInput))
				{
					rawJsonContentStr_ = textBuf.data();
				}

				ImGui::EndTabItem();
			}

			// 2. 登録されたすべての C++ オブジェクトエディタをタブとして表示
			for (const auto& [name, editable] : editors_)
			{
				if (!editable) { continue; }

				if (ImGui::BeginTabItem(name.c_str()))
				{
					selectedItem_ = name;

					ImGui::PushID(editable);
					editable->DrawOptions();
					editable->DrawImGui();
					ImGui::PopID();

					ImGui::EndTabItem();
				}
			}
			ImGui::EndTabBar();
		}
	}, DebugUIArea::Project);
#endif
}

void JsonEditor::Finalize()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->UnregisterDebugUI(this);
	}
#endif
	// エディタリストをクリア
	editors_.clear();
	sharedEditors_.clear();
	// シングルトンインスタンスを解放
	instance_.reset();
}

void JsonEditor::Register(const std::string& name, JsonEditableBase* editor)
{
	if (editor)
	{
		editors_[name] = editor;
		selectedItem_ = name;
	}
}

void JsonEditor::Register(const std::string& name, std::shared_ptr<JsonEditableBase> editor)
{
	if (editor)
	{
		sharedEditors_[name] = editor;
		editors_[name] = editor.get();
		selectedItem_ = name;
	}
}

void JsonEditor::Unregister(JsonEditableBase* editor)
{
	if (!editor) return;

	// editors_ から削除
	for (auto it = editors_.begin(); it != editors_.end();)
	{
		if (it->second == editor)
		{
			if (selectedItem_ == it->first)
			{
				selectedItem_ = "";
			}
			it = editors_.erase(it);
		}
		else
		{
			++it;
		}
	}

	// sharedEditors_ からも削除
	for (auto it = sharedEditors_.begin(); it != sharedEditors_.end();)
	{
		if (it->second.get() == editor)
		{
			it = sharedEditors_.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void JsonEditor::RenderEditUI()
{
}

void JsonEditor::SaveAll()
{
}
} // namespace KCE
