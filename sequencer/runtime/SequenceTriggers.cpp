#include "sequencer/runtime/SequenceTriggers.h"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "base/Logger.h"
#include "base/PathManager.h"
#include "gameobject/base/GameObject.h"
#include "gameobject/manager/GameObjectManager.h"
#include "sequencer/core/BindingContext.h"
#include "sequencer/runtime/CutsceneManager.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace KCE
{
namespace
{
std::filesystem::path FullPath(const std::string& relative)
{
	return PathManager::GetApplicationResourceRoot() / "json" / relative;
}

template<class T>
T Read(const nlohmann::json& j, const char* key, T fallback)
{
	if (j.contains(key))
	{
		try { return j[key].get<T>(); }
		catch (...) {}
	}
	return fallback;
}
} // namespace

bool SequenceTriggers::Load(const std::string& relativePath)
{
	entries_.clear();
	path_ = relativePath;
	const std::filesystem::path path = FullPath(relativePath);
	std::ifstream file(path);
	if (!file)
	{
		return false;
	}
	nlohmann::json json = nlohmann::json::parse(file, nullptr, false);
	if (json.is_discarded() || !json.contains("triggers") || !json["triggers"].is_array())
	{
		Logger::Log("[Triggers] 読めませんでした: " + relativePath + "\n", Logger::LogLevel::Error);
		return false;
	}
	for (const auto& item : json["triggers"])
	{
		Entry entry;
		entry.event = Read<std::string>(item, "event", "");
		entry.sequence = Read<std::string>(item, "sequence", "");
		if (entry.event.empty() || entry.sequence.empty())
		{
			continue;
		}
		entry.originRole = Read<std::string>(item, "originRole", "");
		entry.blendIn = Read<float>(item, "blendIn", 0.5f);
		entry.blendOut = Read<float>(item, "blendOut", 0.5f);
		entry.lockInput = Read<bool>(item, "lockInput", false);
		entry.skippable = Read<bool>(item, "skippable", false);
		entries_.push_back(std::move(entry));
	}
	return true;
}

bool SequenceTriggers::Save() const
{
	if (path_.empty())
	{
		return false;
	}
	nlohmann::json list = nlohmann::json::array();
	for (const Entry& e : entries_)
	{
		nlohmann::json item = { { "event", e.event }, { "sequence", e.sequence } };
		if (!e.originRole.empty()) { item["originRole"] = e.originRole; }
		item["blendIn"] = e.blendIn;
		item["blendOut"] = e.blendOut;
		item["lockInput"] = e.lockInput;
		item["skippable"] = e.skippable;
		list.push_back(std::move(item));
	}
	std::ofstream file(FullPath(path_));
	if (!file)
	{
		return false;
	}
	file << nlohmann::json{ { "triggers", list } }.dump(2);
	return true;
}

void SequenceTriggers::SetProvider(const std::string& role, std::function<GameObject*()> provider)
{
	providers_[role] = std::move(provider);
}

bool SequenceTriggers::Fire(const std::string& eventName) const
{
	if (!CutsceneManager::HasInstance())
	{
		return false;
	}
	bool played = false;
	for (const Entry& entry : entries_)
	{
		if (entry.event != eventName)
		{
			continue;
		}
		CutsceneRequest request;
		request.sequencePath = entry.sequence;
		request.blendInTime = entry.blendIn;
		request.blendOutTime = entry.blendOut;
		request.lockInput = entry.lockInput;
		request.skippable = entry.skippable;

		// 提供役は、いま発火した瞬間に引く（患者や部位は入れ替わるため）
		std::unordered_map<std::string, GameObject*> resolved;
		for (const auto& [role, provider] : providers_)
		{
			if (provider)
			{
				if (GameObject* object = provider())
				{
					resolved[role] = object;
				}
			}
		}
		request.bind = [resolved](BindingContext& ctx)
		{
			for (const auto& [role, object] : resolved)
			{
				ctx.BindGameObject(role, object);
			}
		};
		if (!entry.originRole.empty())
		{
			const auto it = resolved.find(entry.originRole);
			if (it != resolved.end())
			{
				request.useOrigin = true;
				request.originPosition = it->second->GetPosition();
				request.originYaw = it->second->GetRotation().y;
			}
			else
			{
				Logger::Log("[Triggers] 原点の役 \"" + entry.originRole + "\" を提供する物がありません\n", Logger::LogLevel::Error);
			}
		}
		std::string error;
		played = CutsceneManager::GetInstance()->Play(request, &error) || played;
	}
	return played;
}

#ifdef USE_IMGUI
void SequenceTriggers::DrawImGui()
{
	ImGui::Text("読み込み元: %s", path_.empty() ? "(なし)" : path_.c_str());
	if (ImGui::BeginTable("##triggers", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
	{
		ImGui::TableSetupColumn("イベント");
		ImGui::TableSetupColumn("シーケンス");
		ImGui::TableSetupColumn("原点の役");
		ImGui::TableSetupColumn("");
		ImGui::TableHeadersRow();
		for (size_t i = 0; i < entries_.size(); ++i)
		{
			const Entry& e = entries_[i];
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::TextUnformatted(e.event.c_str());
			ImGui::TableNextColumn(); ImGui::TextUnformatted(e.sequence.c_str());
			ImGui::TableNextColumn(); ImGui::TextUnformatted(e.originRole.c_str());
			ImGui::TableNextColumn();
			ImGui::PushID(static_cast<int>(i));
			if (ImGui::SmallButton("再生")) { Fire(e.event); }
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	if (ImGui::Button("読み直す")) { Load(path_); }
}
#endif
} // namespace KCE
