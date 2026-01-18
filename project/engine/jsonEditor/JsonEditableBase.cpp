#include "JsonEditableBase.h"

#include <fstream>

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
		Logger::Log("Failed to open JSON file: " + path);
		return false;
	}

	nlohmann::json json;
	ifs >> json;

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

	return true;
}

bool JsonEditableBase::SaveJson(const std::string& path) const
{
	std::string fullPath = dirPath + fileName;
	nlohmann::json json;
	
	// すべての登録済みプロパティをJSONに変換
	for (auto& [key, getter] : getters_)
	{
		json[key] = getter();
	}
	
	// ファイルに書き出し
	std::ofstream ofs(fullPath);
	if (!ofs)
	{
		Logger::Log("Failed to open JSON file for writing: " + path);
		return false;
	}

	// 整形して出力
	ofs << json.dump(kJsonIndent);
	return true;
}

void JsonEditableBase::DrawImGui()
{
	ImGui::SeparatorText("Settings");
	// 登録済みの描画関数を実行
	for (auto& [name, drawer] : drawers_)
	{
		drawer();
	}
}

void JsonEditableBase::DrawOptions()
{
	ImGui::SeparatorText("Options");
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