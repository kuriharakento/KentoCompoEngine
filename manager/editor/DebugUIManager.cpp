#include "DebugUIManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_internal.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <unordered_map>
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
// 置き場所が見つからないウィンドウは画面の真ん中に出す
constexpr float kCenterPivot = 0.5f;

constexpr char kSettingsTypeName[] = "DebugUI";
constexpr char kGlobalSettingsName[] = "GlobalSettings";
constexpr char kSettingsPagePrefix[] = "settings_page=";
constexpr char kInspectorWindowName[] = "Inspector";
constexpr char kSettingsWindowName[] = "Settings";

/**
 * @brief imgui.ini に覚えておく中身
 * @details ImGui は終了時にも imgui.ini を書き出す。そのときマネージャーが先に消えていても
 *          中身が残るよう、マネージャーの外（static）に持つ。
 */
struct PersistedSettings
{
	std::unordered_map<std::string, bool> visibility;	// ウィンドウ名 → 表示
	float uiScale = 1.0f;
	bool hasUiScale = false;
	bool showConsole = true;
	std::string settingsPage;
};
PersistedSettings s_persisted;

size_t ToIndex(EditorDock dock)
{
	return static_cast<size_t>(dock);
}

/**
 * @brief text が needle を含むか。大文字と小文字は区別しない
 * @details 毎フレームの検索で呼ぶので、小文字にした文字列は作らずに1文字ずつ比べる。
 */
bool ContainsIgnoreCase(const std::string& text, const char* needle)
{
	const size_t needleLength = std::strlen(needle);
	if (needleLength == 0)
	{
		return true;
	}
	for (size_t start = 0; start + needleLength <= text.size(); ++start)
	{
		size_t matched = 0;
		while (matched < needleLength &&
			std::tolower(static_cast<unsigned char>(text[start + matched])) == std::tolower(static_cast<unsigned char>(needle[matched])))
		{
			++matched;
		}
		if (matched == needleLength)
		{
			return true;
		}
	}
	return false;
}

/** @brief その置き場所に必ずあるウィンドウ名。無ければ nullptr */
const char* GetFixedWindowName(EditorDock dock)
{
	switch (dock)
	{
	case EditorDock::Right: return kInspectorWindowName;
	case EditorDock::RightBottom: return kSettingsWindowName;
	default: return nullptr;
	}
}
}

void DebugUIManager::Initialize()
{
	Clear();
	uiScale_ = 1.0f;
	previousUiScale_ = 1.0f;
	resetLayoutRequested_ = false;

	// imgui.ini の [DebugUI] の読み書き口。読み込みは最初の NewFrame で行われるので、それより前に登録しておく
	if (ImGui::GetCurrentContext() && !ImGui::FindSettingsHandler(kSettingsTypeName))
	{
		ImGuiSettingsHandler handler;
		handler.TypeName = kSettingsTypeName;
		handler.TypeHash = ImHashStr(kSettingsTypeName);
		handler.ReadOpenFn = &DebugUIManager::ReadSettingsOpen;
		handler.ReadLineFn = &DebugUIManager::ReadSettingsLine;
		handler.ApplyAllFn = &DebugUIManager::ApplySettings;
		handler.WriteAllFn = &DebugUIManager::WriteSettings;
		ImGui::AddSettingsHandler(&handler);
	}
}

void DebugUIManager::Finalize()
{
	Clear();
	instance_.reset();
}

void DebugUIManager::RegisterWindow(void* owner, const std::string& name, std::function<void()> draw, EditorDock dock, bool defaultVisible)
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
	// シーンを切り替えて登録し直したときも、覚えている表示状態を使う
	const auto saved = s_persisted.visibility.find(name);
	const bool visible = saved != s_persisted.visibility.end() ? saved->second : defaultVisible;
	windows_.push_back({ owner, name, dock, std::move(draw), visible, defaultVisible });
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
			page.displayName = category + "/" + name;
			page.draw = std::move(draw);
			RebuildSettingsCategories();
			return;
		}
	}
	settingsPages_.push_back({ owner, category, name, category + "/" + name, std::move(draw) });
	RebuildSettingsCategories();
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
	RebuildSettingsCategories();
}

void DebugUIManager::Clear()
{
	windows_.clear();
	settingsPages_.clear();
	settingsCategories_.clear();
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
		DockOnFirstOpen(window.name, window.dock);
		const bool wasVisible = window.visible;
		if (ImGui::Begin(window.name.c_str(), &window.visible))
		{
			window.draw();
		}
		ImGui::End();
		if (wasVisible != window.visible)
		{
			SaveSettings();
		}
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
	DockOnFirstOpen(kInspectorWindowName, EditorDock::Right);
	ImGui::Begin(kInspectorWindowName);
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
	DockOnFirstOpen(kSettingsWindowName, EditorDock::RightBottom);
	ImGui::Begin(kSettingsWindowName);
	ImGui::InputTextWithHint("##settings_filter", "Search", settingsFilter_.data(), settingsFilter_.size());
	ImGui::BeginChild("SettingsList", ImVec2(kSettingsListWidth, 0.0f), true);
	const bool filtering = settingsFilter_[0] != '\0';
	for (const auto& category : settingsCategories_)
	{
		// 検索中は、当てはまるページが無い分類ごと隠し、当てはまる分類は開いて見せる
		const bool hasMatch = std::any_of(settingsPages_.begin(), settingsPages_.end(),
			[this, &category](const SettingsPage& page) { return page.category == category && MatchesSettingsFilter(page); });
		if (!hasMatch)
		{
			continue;
		}
		if (filtering)
		{
			ImGui::SetNextItemOpen(true);
		}
		if (!ImGui::TreeNodeEx(category.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
		{
			continue;
		}
		for (const auto& page : settingsPages_)
		{
			if (page.category != category || !MatchesSettingsFilter(page))
			{
				continue;
			}
			ImGui::PushID(&page);
			if (ImGui::Selectable(page.name.c_str(), selectedSettingsPage_ == page.name))
			{
				selectedSettingsPage_ = page.name;
				SaveSettings();
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
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

void DebugUIManager::RebuildSettingsCategories()
{
	settingsCategories_.clear();
	for (const auto& page : settingsPages_)
	{
		if (std::find(settingsCategories_.begin(), settingsCategories_.end(), page.category) == settingsCategories_.end())
		{
			settingsCategories_.push_back(page.category);
		}
	}
}

bool DebugUIManager::MatchesSettingsFilter(const SettingsPage& page) const
{
	return settingsFilter_[0] == '\0' || ContainsIgnoreCase(page.displayName, settingsFilter_.data());
}

void DebugUIManager::DockOnFirstOpen(const std::string& name, EditorDock dock)
{
	// imgui.ini に設定がある（＝ユーザーが置いた場所がある）ウィンドウには触らない
	if (ImGui::FindWindowByName(name.c_str()) || ImGui::FindWindowSettingsByID(ImHashStr(name.c_str())))
	{
		return;
	}

	const auto dockNextTo = [&name](const char* otherName)
	{
		if (name == otherName)
		{
			return false;
		}
		const ImGuiWindow* other = ImGui::FindWindowByName(otherName);
		if (!other || other->DockId == 0)
		{
			return false;
		}
		ImGui::SetNextWindowDockID(other->DockId, ImGuiCond_FirstUseEver);
		return true;
	};
	for (const auto& window : windows_)
	{
		if (window.dock == dock && dockNextTo(window.name.c_str()))
		{
			return;
		}
	}
	if (const char* fixedName = GetFixedWindowName(dock); fixedName && dockNextTo(fixedName))
	{
		return;
	}
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(kCenterPivot, kCenterPivot));
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
	if (const char* fixedName = GetFixedWindowName(dock))
	{
		names.push_back(fixedName);
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
		if (ImGui::MenuItem(window.name.c_str(), nullptr, &window.visible))
		{
			SaveSettings();
		}
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
	SaveSettings();
}

void DebugUIManager::SetUIScale(float scale)
{
	uiScale_ = (std::clamp)(scale, kMinimumUiScale, kMaximumUiScale);
	ImGui::GetIO().FontGlobalScale = uiScale_;
	ImGui::GetStyle().ScaleAllSizes(uiScale_ / previousUiScale_);
	previousUiScale_ = uiScale_;
	SaveSettings();
}

void DebugUIManager::SetShowConsole(bool show)
{
	if (showConsole_ == show)
	{
		return;
	}
	showConsole_ = show;
	SaveSettings();
}

void DebugUIManager::SaveSettings()
{
	for (const auto& window : windows_)
	{
		s_persisted.visibility[window.name] = window.visible;
	}
	s_persisted.uiScale = uiScale_;
	s_persisted.hasUiScale = true;
	s_persisted.showConsole = showConsole_;
	s_persisted.settingsPage = selectedSettingsPage_;
	// 実際の書き出しは ImGui が少し後（io.IniSavingRate）にまとめて行う
	ImGui::MarkIniSettingsDirty();
}

void* DebugUIManager::ReadSettingsOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name)
{
	if (std::strcmp(name, kGlobalSettingsName) == 0)
	{
		return &s_persisted;
	}
	// unordered_map の要素のアドレスは、他の要素を足しても変わらない
	return &s_persisted.visibility[name];
}

void DebugUIManager::ReadSettingsLine(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line)
{
	int value = 0;
	if (entry == &s_persisted)
	{
		float scale = 0.0f;
		if (sscanf_s(line, "ui_scale=%f", &scale) == 1)
		{
			s_persisted.uiScale = scale;
			s_persisted.hasUiScale = true;
		}
		else if (sscanf_s(line, "console=%d", &value) == 1)
		{
			s_persisted.showConsole = value != 0;
		}
		else if (std::strncmp(line, kSettingsPagePrefix, std::strlen(kSettingsPagePrefix)) == 0)
		{
			s_persisted.settingsPage = line + std::strlen(kSettingsPagePrefix);
		}
		return;
	}
	// 古い形式の area= などは読み飛ばす
	if (sscanf_s(line, "visible=%d", &value) == 1)
	{
		*static_cast<bool*>(entry) = value != 0;
	}
}

void DebugUIManager::ApplySettings(ImGuiContext*, ImGuiSettingsHandler*)
{
	if (!HasInstance())
	{
		return;
	}
	DebugUIManager* manager = instance_.get();
	for (auto& window : manager->windows_)
	{
		const auto saved = s_persisted.visibility.find(window.name);
		if (saved != s_persisted.visibility.end())
		{
			window.visible = saved->second;
		}
	}
	manager->showConsole_ = s_persisted.showConsole;
	if (!s_persisted.settingsPage.empty())
	{
		manager->selectedSettingsPage_ = s_persisted.settingsPage;
	}
	if (s_persisted.hasUiScale)
	{
		manager->SetUIScale(s_persisted.uiScale);
	}
}

void DebugUIManager::WriteSettings(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buffer)
{
	// マネージャーが生きていれば今の状態を写してから書く。終了時に先に消えていたら、覚えている分をそのまま書く
	if (HasInstance())
	{
		DebugUIManager* manager = instance_.get();
		for (const auto& window : manager->windows_)
		{
			s_persisted.visibility[window.name] = window.visible;
		}
		s_persisted.uiScale = manager->uiScale_;
		s_persisted.hasUiScale = true;
		s_persisted.showConsole = manager->showConsole_;
		s_persisted.settingsPage = manager->selectedSettingsPage_;
	}

	buffer->appendf("[%s][%s]\n", handler->TypeName, kGlobalSettingsName);
	buffer->appendf("ui_scale=%.2f\n", s_persisted.uiScale);
	buffer->appendf("console=%d\n", s_persisted.showConsole ? 1 : 0);
	buffer->appendf("%s%s\n", kSettingsPagePrefix, s_persisted.settingsPage.c_str());
	buffer->append("\n");
	for (const auto& [name, visible] : s_persisted.visibility)
	{
		buffer->appendf("[%s][%s]\n", handler->TypeName, name.c_str());
		buffer->appendf("visible=%d\n", visible ? 1 : 0);
		buffer->append("\n");
	}
}
#endif
} // namespace KCE
