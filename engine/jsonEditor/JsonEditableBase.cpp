#include "JsonEditableBase.h"

#include <fstream>
#include <filesystem>

// JSONインデント幅
constexpr int kJsonIndent = 4;

JsonEditableBase::~JsonEditableBase()
{
	// 登録済みsetterをクリア
	setters_.clear();
	// 登録済みgetterをクリア
	getters_.clear();
	// 登録済みdrawerをクリア
	drawers_.clear();
	// 登録済みメンバーをクリア
    registeredMembers_.clear();
}

bool JsonEditableBase::LoadJson(const std::string& path)
{
	fileName = path;
	std::string fullPath = dirPath + fileName;
	
	// JSONファイルを開く
	std::ifstream ifs(fullPath);
	if (!ifs)
	{
		Logger::Log("[JSON Error] Failed to open JSON file: " + fullPath + "\n");
		return false;
	}

	nlohmann::json json;
	try
	{
		ifs >> json;
	}
	catch (const std::exception& e)
	{
		Logger::Log("[JSON Error] Parsing exception in file '" + fullPath + "': " + std::string(e.what()) + "\n");
		return false;
	}

#ifdef _DEBUG
	// デバッグ: 読み込んだファイルパスとobjects配列サイズ
	Logger::Log("LoadJson path: " + fullPath + "\n");
	if (json.contains("objects"))
	{
		if (json["objects"].is_array())
		{
			Logger::Log("Raw JSON 'objects' count: " + std::to_string(json["objects"].size()) + "\n");
		}
		else
		{
			Logger::Log("'objects' is not an array\n");
		}
	}
	else
	{
		Logger::Log("No 'objects' key in JSON\n");
	}
#endif

    // 登録済みsetterのみ値をセット
    for (const auto& [key, setter] : setters_)
    {
		try
		{
			const nlohmann::json* current = &json;
			size_t pos = 0, next;
			std::string keyPath = key;
			bool found = true;
        
			// ドット区切りのパスを辿る
			while ((next = keyPath.find('.', pos)) != std::string::npos)
			{
				std::string token = keyPath.substr(pos, next - pos);
				// 配列インデックス対応
				size_t arrPos = token.find('[');
				if (arrPos != std::string::npos)
				{
					std::string arrName = token.substr(0, arrPos);
					size_t arrEnd = token.find(']', arrPos);
					int idx = std::stoi(token.substr(arrPos + 1, arrEnd - arrPos - 1));
					if (current->contains(arrName) && (*current)[arrName].is_array() && idx < (*current)[arrName].size())
					{
						current = &(*current)[arrName][idx];
					}
					else
					{
						found = false;
						break;
					}
				}
				else
				{
					if (current->contains(token))
						current = &(*current)[token];
					else
					{
						found = false;
						break;
					}
				}
				pos = next + 1;
			}
        
			// 最後のトークンを処理
			std::string lastToken = keyPath.substr(pos);
			// 配列インデックス対応
			size_t arrPos = lastToken.find('[');
			if (arrPos != std::string::npos)
			{
				std::string arrName = lastToken.substr(0, arrPos);
				size_t arrEnd = lastToken.find(']', arrPos);
				int idx = std::stoi(lastToken.substr(arrPos + 1, arrEnd - arrPos - 1));
				if (current->contains(arrName) && (*current)[arrName].is_array() && idx < (*current)[arrName].size())
				{
					SetValue(key, (*current)[arrName][idx]);
				}
			}
			else if (found && current->contains(lastToken))
			{
				SetValue(key, (*current)[lastToken]);
			}
		}
		catch (const std::exception& e)
		{
			Logger::Log("[JSON Warning] Value assignment failed for key '" + key + "': " + std::string(e.what()) + "\n");
		}
    }

	return true;
}

bool JsonEditableBase::SaveJson(const std::string& path) const
{
	std::string fullPath = dirPath + fileName;
	nlohmann::json json;
	
	// すべての登録済みプロパティをJSONに変換
	for (auto& [key, getter] : getters_)
	{
		try
		{
			json[key] = getter();
		}
		catch (const std::exception& e)
		{
			Logger::Log("[JSON Error] Failed to serialize key '" + key + "': " + std::string(e.what()) + "\n");
		}
	}
	
	// ファイルに書き出し
	std::ofstream ofs(fullPath);
	if (!ofs)
	{
		Logger::Log("[JSON Error] Failed to open JSON file for writing: " + fullPath + "\n");
		return false;
	}

	try
	{
		// 整形して出力
		ofs << json.dump(kJsonIndent);
	}
	catch (const std::exception& e)
	{
		Logger::Log("[JSON Error] Exception during JSON write to file '" + fullPath + "': " + std::string(e.what()) + "\n");
		return false;
	}

	return true;
}

void JsonEditableBase::DrawImGui()
{
	ImGui::SeparatorText("Settings");

	// フィルターとコントロール行
	filter_.Draw("Filter", 180.0f);
	ImGui::SameLine();
	if (ImGui::Button("Expand All"))
	{
		expandCollapseTrigger_ = 1;
	}
	ImGui::SameLine();
	if (ImGui::Button("Collapse All"))
	{
		expandCollapseTrigger_ = 2;
	}

	bool inTable = false;

	// 登録順に従って描画
	for (const std::string& name : registrationOrder_)
	{
		// フィルターが有効な場合はマッチするかチェック
		if (!filter_.PassFilter(name.c_str()))
		{
			continue;
		}

		auto itDrawer = drawers_.find(name);
		if (itDrawer == drawers_.end())
		{
			continue;
		}

		bool isComp = isComposite_.at(name);

		if (isComp)
		{
			// テーブルが開いていれば閉じる
			if (inTable)
			{
				ImGui::EndTable();
				inTable = false;
			}
			
			// 複合型の描画（個別のツリーノードなど）
			itDrawer->second();
		}
		else
		{
			// 基本型：テーブル内に配置
			if (!inTable)
			{
				if (ImGui::BeginTable("BasicPropertiesTable", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg))
				{
					ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.0f);
					ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableHeadersRow();
					inTable = true;
				}
			}

			if (inTable)
			{
				itDrawer->second();
			}
		}
	}

	if (inTable)
	{
		ImGui::EndTable();
	}

	// トリガーのリセット
	expandCollapseTrigger_ = 0;
}

void JsonEditableBase::DrawOptions()
{
	ImGui::SeparatorText("Options");

	// 現在のファイル名の編集用バッファ
	char fileBuf[256];
	strncpy_s(fileBuf, fileName.c_str(), sizeof(fileBuf));
	fileBuf[sizeof(fileBuf) - 1] = '\0';
	
	ImGui::PushItemWidth(250.0f);
	if (ImGui::InputText("Target File Name", fileBuf, sizeof(fileBuf)))
	{
		fileName = fileBuf;
	}
	ImGui::PopItemWidth();

	// ファイルリストの取得
	std::vector<std::string> jsonFiles;
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
	catch (const std::exception& e)
	{
		Logger::Log("[JSON Error] Exception scanning directory: " + std::string(e.what()) + "\n");
	}

	// ドロップダウンでファイル一覧を表示
	if (!jsonFiles.empty())
	{
		ImGui::SameLine();
		ImGui::PushItemWidth(200.0f);
		if (ImGui::BeginCombo("##FileSelect", fileName.c_str()))
		{
			for (const auto& file : jsonFiles)
			{
				bool isSelected = (fileName == file);
				if (ImGui::Selectable(file.c_str(), isSelected))
				{
					fileName = file;
					LoadJson(fileName); // 選択時に即時ロードする
				}
				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
	}

	ImGui::Spacing();

	// 保存ボタン
	if (ImGui::Button("Save Json"))
	{
		SaveJson(fileName);
	}
	ImGui::SameLine();
	// 読込ボタン
	if (ImGui::Button("Load Json"))
	{
		LoadJson(fileName);
	}
}

void JsonEditableBase::SetValue(const std::string& key, const nlohmann::json& value)
{
    auto it = setters_.find(key);
    if (it != setters_.end())
    {
        it->second(value);
    }
}

nlohmann::json JsonEditableBase::Serialize() const
{
	nlohmann::json json = nlohmann::json::object();
	for (const auto& [key, getter] : getters_)
	{
		try
		{
			json[key] = getter();
		}
		catch (const std::exception& e)
		{
			Logger::Log("[JSON Error] Failed to serialize key '" + key + "': " + std::string(e.what()) + "\n");
		}
	}
	return json;
}

void JsonEditableBase::Deserialize(const nlohmann::json& json)
{
	for (const auto& [key, setter] : setters_)
	{
		if (json.contains(key))
		{
			try
			{
				setter(json.at(key));
			}
			catch (const std::exception& e)
			{
				Logger::Log("[JSON Warning] Value assignment failed for key '" + key + "': " + std::string(e.what()) + "\n");
			}
		}
	}
}