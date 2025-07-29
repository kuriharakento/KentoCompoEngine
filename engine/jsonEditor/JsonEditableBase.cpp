#include "JsonEditableBase.h"

#include <fstream>


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
	std::ifstream ifs(fullPath);
	if (!ifs)
	{
		Logger::Log("Failed to open JSON file: " + path);
		return false;
	}

	nlohmann::json json;
	ifs >> json;

    // 登録済みsetterのみ値をセット
    for (const auto& [key, setter] : setters_)
    {
        const nlohmann::json* current = &json;
        size_t pos = 0, next;
        std::string keyPath = key;
        bool found = true;
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
	for (auto& [key, getter] : getters_)
	{
		json[key] = getter();
	}
	std::ofstream ofs(fullPath);
	if (!ofs)
	{
		Logger::Log("Failed to open JSON file for writing: " + path);
		return false;
	}

	ofs << json.dump(4);
	return true;
}

void JsonEditableBase::DrawImGui()
{
	ImGui::SeparatorText("Settings");
	for (auto& [name, drawer] : drawers_)
	{
		drawer();
	}
}

void JsonEditableBase::DrawOptions()
{
	ImGui::SeparatorText("Options");
	if (ImGui::Button("Save Json"))
	{
		SaveJson(fileName);
	}
	ImGui::SameLine();
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