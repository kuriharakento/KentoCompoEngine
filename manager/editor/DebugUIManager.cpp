#include "DebugUIManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include <algorithm>
#include <cstring>
#endif

namespace KCE
{
std::unique_ptr<DebugUIManager> DebugUIManager::instance_ = nullptr;

DebugUIManager* DebugUIManager::GetInstance()
{
#ifdef USE_IMGUI
	if (!instance_)
	{
		instance_ = std::make_unique<DebugUIManager>();
	}
	return instance_.get();
#else
	return nullptr;
#endif
}

bool DebugUIManager::HasInstance()
{
	return instance_ != nullptr;
}

#ifdef USE_IMGUI
namespace
{
constexpr float kMinimumUiScale = 0.5f;
constexpr float kMaximumUiScale = 3.0f;
constexpr float kSettingsListWidth = 180.0f;

size_t ToIndex(EditorDock dock)
{
	return static_cast<size_t>(dock);
}
}

void DebugUIManager::Initialize()
{
	Clear();
	uiScale_ = 1.0f;
	resetLayoutRequested_ = false;
}

void DebugUIManager::Finalize()
{
	Clear();
	instance_.reset();
}

void DebugUIManager::RegisterWindow(void* owner, const std::string& name, EditorDock dock, std::function<void()> draw, bool defaultVisible)
{
	if (!owner || name.empty() || !draw)
	{
		return;
	}
	for (auto& window : windows_)
	{
		if (window.owner == owner && window.name == name)
		{
			window.dock = dock;
			window.draw = std::move(draw);
			return;
		}
	}
	const auto saved = savedVisibility_.find(name);
	const bool visible = saved != savedVisibility_.end() ? saved->second : defaultVisible;
	windows_.push_back({ owner, name, dock, std::move(draw), visible, defaultVisible });
}

void DebugUIManager::RegisterWindow(void* owner, const std::string& name, std::function<void()> draw, EditorDock dock, bool defaultVisible)
{
	RegisterWindow(owner, name, dock, std::move(draw), defaultVisible);
}

void DebugUIManager::RegisterSettingsPage(void* owner, const std::string& category, const std::string& name, std::function<void()> draw)
{
	if (!owner || category.empty() || name.empty() || !draw)
	{
		return;
	}
	for (auto& page : settingsPages_)
	{
		if (page.owner == owner && page.name == name)
		{
			page.category = category;
			page.draw = std::move(draw);
			return;
		}
	}
	settingsPages_.push_back({ owner, category, name, std::move(draw) });
	if (selectedSettingsPage_.empty())
	{
		selectedSettingsPage_ = name;
	}
}

void DebugUIManager::RegisterInspector(void* owner, SelectionKind kind, std::function<void(const SelectionItem&)> draw)
{
	if (!owner || kind == SelectionKind::None || !draw)
	{
		return;
	}
	for (auto& page : inspectorPages_)
	{
		if (page.owner == owner && page.kind == kind)
		{
			page.draw = std::move(draw);
			return;
		}
	}
	inspectorPages_.push_back({ owner, kind, std::move(draw) });
}

void DebugUIManager::RegisterSceneOverlay(void* owner, std::function<void()> draw)
{
	if (!owner || !draw)
	{
		return;
	}
	for (auto& overlay : overlays_)
	{
		if (overlay.owner == owner)
		{
			overlay.draw = std::move(draw);
			return;
		}
	}
	overlays_.push_back({ owner, std::move(draw) });
}

void DebugUIManager::Unregister(void* owner)
{
	if (!owner)
	{
		return;
	}
	const auto removeOwner = [owner](const auto& item) { return item.owner == owner; };
	std::erase_if(windows_, removeOwner);
	std::erase_if(settingsPages_, removeOwner);
	std::erase_if(inspectorPages_, removeOwner);
	std::erase_if(overlays_, removeOwner);
}

void DebugUIManager::Clear()
{
	windows_.clear();
	settingsPages_.clear();
	inspectorPages_.clear();
	overlays_.clear();
	for (auto& names : dockWindowNames_)
	{
		names.clear();
	}
}

void DebugUIManager::Draw()
{
	for (auto& window : windows_)
	{
		if (!window.visible)
		{
			continue;
		}
		if (ImGui::Begin(window.name.c_str(), &window.visible))
		{
			window.draw();
		}
		ImGui::End();
	}
	DrawInspector();
	DrawSettings();
}

void DebugUIManager::DrawSceneOverlays()
{
	for (const auto& overlay : overlays_)
	{
		overlay.draw();
	}
}

void DebugUIManager::DrawInspector()
{
	ImGui::Begin("Inspector");
	const SelectionItem& selected = SelectionContext::GetInstance()->GetPrimary();
	if (selected.kind == SelectionKind::None)
	{
		ImGui::TextUnformatted("Hierarchy で物を選ぶか、Sequencer でトラックやキーを選ぶと、ここに詳細が出ます。");
	}
	else
	{
		for (const auto& page : inspectorPages_)
		{
			if (page.kind == selected.kind)
			{
				page.draw(selected);
				ImGui::End();
				return;
			}
		}
		ImGui::TextUnformatted("この種類の詳細表示はまだ登録されていません。");
	}
	ImGui::End();
}

void DebugUIManager::DrawSettings()
{
	ImGui::Begin("Settings");
	ImGui::InputTextWithHint("##settings_filter", "Search", settingsFilter_.data(), settingsFilter_.size());
	ImGui::BeginChild("SettingsList", ImVec2(kSettingsListWidth, 0.0f), true);
	for (size_t index = 0; index < settingsPages_.size(); ++index)
	{
		const auto& page = settingsPages_[index];
		if (settingsFilter_[0] != '\0' && page.name.find(settingsFilter_.data()) == std::string::npos && page.category.find(settingsFilter_.data()) == std::string::npos)
		{
			continue;
		}
		if (ImGui::Selectable((page.category + "/" + page.name).c_str(), selectedSettingsPage_ == page.name))
		{
			selectedSettingsPage_ = page.name;
		}
	}
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("SettingsContent", ImVec2(0.0f, 0.0f), true);
	for (const auto& page : settingsPages_)
	{
		if (page.name == selectedSettingsPage_)
		{
			page.draw();
			break;
		}
	}
	ImGui::EndChild();
	ImGui::End();
}

const std::vector<std::string>& DebugUIManager::GetDockWindowNames(EditorDock dock)
{
	auto& names = dockWindowNames_[ToIndex(dock)];
	names.clear();
	for (const auto& window : windows_)
	{
		if (window.dock == dock)
		{
			names.push_back(window.name);
		}
	}
	if (dock == EditorDock::Right)
	{
		names.push_back("Inspector");
	}
	if (dock == EditorDock::RightBottom)
	{
		names.push_back("Settings");
	}
	return names;
}

void DebugUIManager::DrawWindowMenu()
{
	if (!ImGui::BeginMenu("Windows"))
	{
		return;
	}
	for (auto& window : windows_)
	{
		ImGui::MenuItem(window.name.c_str(), nullptr, &window.visible);
	}
	ImGui::EndMenu();
}

void DebugUIManager::RequestLayoutReset()
{
	for (auto& window : windows_)
	{
		window.visible = window.defaultVisible;
	}
	resetLayoutRequested_ = true;
}

void DebugUIManager::SetUIScale(float scale)
{
	uiScale_ = (std::clamp)(scale, kMinimumUiScale, kMaximumUiScale);
	ImGui::GetIO().FontGlobalScale = uiScale_;
}
#endif
} // namespace KCE
