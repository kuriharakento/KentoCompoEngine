#include "JsonEditorManager.h"
#include "DebugUIManager.h"
#include "imgui/imgui.h"
#include <filesystem>
#include <fstream>
#include <sstream>

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
						Logger::Log("[JSON Error] Failed to open raw JSON file: " + dirPath + rawJsonFileName_ + "\n");
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
						Logger::Log("[JSON Error] Failed to save raw JSON file: " + dirPath + rawJsonFileName_ + "\n");
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

					ImGui::PushID(editable.get());
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
