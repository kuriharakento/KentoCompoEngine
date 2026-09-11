#include "DebugUIManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_internal.h"
#include "externals/nlohmann/json.hpp"
#include <algorithm>
#include <fstream>

#endif

namespace KCE
{

// instance_ の実体は、非ImGui環境でのリンクエラーを防ぐために常に定義する
std::unique_ptr<DebugUIManager> DebugUIManager::instance_ = nullptr;

DebugUIManager* DebugUIManager::GetInstance()
{
#ifdef USE_IMGUI
	if (instance_ == nullptr)
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

// ------ ここから下は ImGui 有効時（Debugビルド等）のみコンパイル ------
#ifdef USE_IMGUI

static std::unordered_map<std::string, DebugUIManager::SavedUIState> s_savedStates;
static float s_prevScale = 1.0f;

namespace
{
struct DebugUIWindowConfig
{
	const char* name;
	const char* category;
	DebugUIDockLocation dockLocation;
	bool visible;
	bool prefixMatch;
};

constexpr DebugUIWindowConfig kWindowConfigs[] = {
	{"GameObject List", "Scene", DebugUIDockLocation::Left, true, false},
	{"SceneManager", "Scene", DebugUIDockLocation::Left, true, false},
	{"Title Scene", "Scene", DebugUIDockLocation::Left, true, false},
	{"Feature Check", "Scene", DebugUIDockLocation::Left, true, false},
	{"GameObject Inspector", "Scene", DebugUIDockLocation::RightTop, true, false},
	{"Sequencer Inspector", "Sequencer", DebugUIDockLocation::RightTop, true, false},
	{"Camera Manager", "Scene", DebugUIDockLocation::RightTop, true, false},
	{"Light Manager", "Scene", DebugUIDockLocation::RightTop, true, false},
	// Sequencer と一緒に使うので、下の段の隣のタブに置く
	{"Cutscene", "Sequencer", DebugUIDockLocation::Bottom, true, false},
	{"Post Process", "Rendering", DebugUIDockLocation::RightBottom, true, false},
	{"Atmosphere Fog", "Rendering", DebugUIDockLocation::RightBottom, true, false},
	{"Light Beams", "Rendering", DebugUIDockLocation::RightBottom, true, false},
	{"Outline", "Rendering", DebugUIDockLocation::RightBottom, true, false},
	{"NPR Shading", "Rendering", DebugUIDockLocation::RightBottom, true, false},
	// 描画の不具合を切り分けるときの道具なので、右下のタブを増やさないよう下の段に置く
	{"Render Pipeline", "Rendering", DebugUIDockLocation::Bottom, true, false},
	{"Sequencer", "Sequencer", DebugUIDockLocation::Bottom, true, false},
	{"Console", "System", DebugUIDockLocation::Bottom, true, false},
	{"Shader Hot Reload", "Rendering", DebugUIDockLocation::Bottom, true, false},
	{"Time Manager", "System", DebugUIDockLocation::RightTop, false, false},
	{"Timer Manager", "System", DebugUIDockLocation::RightTop, false, false},
	{"Debug Camera", "Scene", DebugUIDockLocation::RightTop, false, false},
	{"TopDownCamera Settings", "Scene", DebugUIDockLocation::RightTop, false, false},
	{"Audio Debug", "System", DebugUIDockLocation::Bottom, false, false},
	{"JSON Editor", "System", DebugUIDockLocation::Bottom, false, false},
	{"Particle Editor", "Effects", DebugUIDockLocation::RightTop, false, false},
	{"Particle Manager", "Effects", DebugUIDockLocation::RightTop, false, false},
	{"CollisionManager Colliders", "System", DebugUIDockLocation::Bottom, false, false},
	{"Font Sprite:", "System", DebugUIDockLocation::Bottom, false, true},
};

const DebugUIWindowConfig* FindWindowConfig(const std::string& name)
{
	for (const auto& config : kWindowConfigs)
	{
		if ((!config.prefixMatch && name == config.name) ||
			(config.prefixMatch && name.starts_with(config.name)))
		{
			return &config;
		}
	}
	return nullptr;
}

DebugUIDockLocation GetAreaDockLocation(DebugUIArea area)
{
	switch (area)
	{
	case DebugUIArea::Hierarchy: return DebugUIDockLocation::Left;
	case DebugUIArea::Inspector: return DebugUIDockLocation::RightTop;
	case DebugUIArea::Console: return DebugUIDockLocation::Bottom;
	case DebugUIArea::Scene: return DebugUIDockLocation::Center;
	case DebugUIArea::Project: return DebugUIDockLocation::Bottom;
	}
	return DebugUIDockLocation::RightTop;
}

DebugUIDockLocation GetDockLocation(const DebugUI& ui)
{
	const auto* config = FindWindowConfig(ui.name);
	return config ? config->dockLocation : GetAreaDockLocation(ui.area);
}
}

void DebugUIManager::Initialize()
{
	debugUIs_.clear();

	// 旧レイアウトファイルのクリーンアップ
	std::remove("debug_ui_layout.json");

	// 設定ハンドラ登録
	ImGuiContext* ctx = ImGui::GetCurrentContext();
	if (ctx != nullptr)
	{
		ImGuiSettingsHandler ini_handler;
		ini_handler.TypeName = "DebugUI";
		ini_handler.TypeHash = ImHashStr("DebugUI");

		ini_handler.ClearAllFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler)
		{
			DebugUIManager::GetInstance()->ClearLoadedStates();
		};
		ini_handler.ReadOpenFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name) -> void*
		{
			if (strcmp(name, "GlobalSettings") == 0)
			{
				return (void*)DebugUIManager::GetInstance();
			}
			return (void*)&(DebugUIManager::GetInstance()->GetOrAddLoadedState(name));
		};
		ini_handler.ReadLineFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line)
		{
			if (entry == DebugUIManager::GetInstance())
			{
				float fval = 1.0f;
				if (sscanf_s(line, "ui_scale=%f", &fval) == 1)
				{
					DebugUIManager::GetInstance()->SetUIScale(fval);
				}
				return;
			}
			auto* state = static_cast<DebugUIManager::SavedUIState*>(entry);
			int val = 0;
			if (sscanf_s(line, "area=%d", &val) == 1)
			{
				state->area = static_cast<DebugUIArea>(val);
			}
			else if (sscanf_s(line, "visible=%d", &val) == 1)
			{
				state->visible = (val != 0);
			}
		};
		ini_handler.ApplyAllFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler)
		{
			DebugUIManager::GetInstance()->ApplyLoadedStatesToActiveUIs();
		};
		ini_handler.WriteAllFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
		{
			DebugUIManager::GetInstance()->WriteAllSettings(buf);
		};

		if (ImGui::FindSettingsHandler("DebugUI") == nullptr)
		{
			ImGui::AddSettingsHandler(&ini_handler);
		}
	}
	resetLayoutRequested_ = false;

	uiScale_ = 1.0f;
	s_prevScale = 1.0f;

	showHierarchy_ = true;
	showInspector_ = true;
	showConsole_ = true;
	showProject_ = true;
}

void DebugUIManager::Finalize()
{
	debugUIs_.clear();
	instance_.reset();
}

void DebugUIManager::RegisterDebugUI([[maybe_unused]] void* owner, [[maybe_unused]] const std::string& name, [[maybe_unused]] std::function<void()> drawFunc, [[maybe_unused]] DebugUIArea area)
{
	if (owner == nullptr || name.empty() || !drawFunc)
	{
		return;
	}

	// 同名UIがあれば上書き
	auto& list = debugUIs_[owner];
	for (auto& ui : list)
	{
		if (ui.name == name)
		{
			ui.drawFunc = drawFunc;
			return;
		}
	}

	// 新規登録（ロード済みの状態を優先）
	DebugUIArea finalArea = area;
	const auto* config = FindWindowConfig(name);
	bool finalVisible = config ? config->visible : true;

	auto it = s_savedStates.find(name);
	if (it != s_savedStates.end())
	{
		finalArea = it->second.area;
		finalVisible = it->second.visible;
	}

	list.push_back({name, drawFunc, finalArea, area, finalVisible});
}

void DebugUIManager::UnregisterDebugUI([[maybe_unused]] void* owner)
{
	if (owner == nullptr)
	{
		return;
	}

	debugUIs_.erase(owner);
}

void DebugUIManager::Clear()
{
	debugUIs_.clear();
}

void DebugUIManager::Draw()
{
	for (auto& [owner, list] : debugUIs_)
	{
		for (auto& ui : list)
		{
			// ギズモだけはScene画像と同じ描画リストが必要なので、Scene内で呼ぶ。
			if (ui.name != "Sequencer Gizmo" && ui.visible)
			{
				const bool wasVisible = ui.visible;
				if (ImGui::Begin(ui.name.c_str(), &ui.visible))
				{
					// 個別UIには手を入れず、ウィンドウ幅に合わせて長文を折り返す。
					ImGui::PushTextWrapPos(0.0f);
					ui.drawFunc();
					ImGui::PopTextWrapPos();
				}
				ImGui::End();
				if (wasVisible != ui.visible)
				{
					SaveLayout();
				}
			}
		}
	}
}

void DebugUIManager::DrawArea([[maybe_unused]] DebugUIArea area)
{
	if (area != DebugUIArea::Scene)
	{
		return;
	}

	for (auto& [owner, list] : debugUIs_)
	{
		for (auto& ui : list)
		{
			if (ui.name == "Sequencer Gizmo" && ui.visible)
			{
				// 見出しや余白を足すと画像領域が広がってスクロールするので、そのまま重ねる。
				ui.drawFunc();
			}
		}
	}
}

bool DebugUIManager::HasVisibleDebugUI(DebugUIArea area) const
{
	for (const auto& [owner, list] : debugUIs_)
	{
		for (const auto& ui : list)
		{
			if (ui.area == area && ui.visible)
			{
				return true;
			}
		}
	}
	return false;
}

void DebugUIManager::DrawWindowMenu()
{
	const auto drawCategory = [this](const char* category)
	{
		bool hasItem = false;
		for (const auto& config : kWindowConfigs)
		{
			if (strcmp(config.category, category) == 0 && strcmp(config.name, "Console") == 0)
			{
				hasItem = true;
			}
			for (const auto& [owner, list] : debugUIs_)
			{
				for (const auto& ui : list)
				{
					if (strcmp(config.category, category) == 0 && FindWindowConfig(ui.name) == &config)
					{
						hasItem = true;
					}
				}
			}
		}
		if (!hasItem || !ImGui::BeginMenu(category))
		{
			return;
		}

		for (const auto& config : kWindowConfigs)
		{
			if (strcmp(config.category, category) != 0)
			{
				continue;
			}
			if (strcmp(config.name, "Console") == 0)
			{
				if (ImGui::MenuItem("Console", nullptr, &showConsole_))
				{
					SaveLayout();
				}
				continue;
			}
			for (auto& [owner, list] : debugUIs_)
			{
				for (auto& ui : list)
				{
					if (FindWindowConfig(ui.name) == &config && ImGui::MenuItem(ui.name.c_str(), nullptr, &ui.visible))
					{
						SaveLayout();
					}
				}
			}
		}
		ImGui::EndMenu();
	};

	for (size_t i = 0; i < IM_ARRAYSIZE(kWindowConfigs); ++i)
	{
		bool alreadyDrawn = false;
		for (size_t previous = 0; previous < i; ++previous)
		{
			if (strcmp(kWindowConfigs[i].category, kWindowConfigs[previous].category) == 0)
			{
				alreadyDrawn = true;
				break;
			}
		}
		if (!alreadyDrawn)
		{
			drawCategory(kWindowConfigs[i].category);
		}
	}

	bool hasOther = false;
	for (const auto& [owner, list] : debugUIs_)
	{
		for (const auto& ui : list)
		{
			hasOther |= FindWindowConfig(ui.name) == nullptr;
		}
	}
	if (hasOther && ImGui::BeginMenu("Other"))
	{
		for (auto& [owner, list] : debugUIs_)
		{
			for (auto& ui : list)
			{
				if (!FindWindowConfig(ui.name) && ImGui::MenuItem(ui.name.c_str(), nullptr, &ui.visible))
				{
					SaveLayout();
				}
			}
		}
		ImGui::EndMenu();
	}
}

std::vector<std::string> DebugUIManager::GetDockWindowNames(DebugUIDockLocation location) const
{
	std::vector<std::string> names;
	for (const auto& [owner, list] : debugUIs_)
	{
		for (const auto& ui : list)
		{
			if (ui.name != "Sequencer Gizmo" && GetDockLocation(ui) == location)
			{
				names.push_back(ui.name);
			}
		}
	}
	if (location == DebugUIDockLocation::Bottom)
	{
		names.push_back("Console");
	}
	std::sort(names.begin(), names.end());
	return names;
}

void DebugUIManager::RequestLayoutReset()
{
	for (auto& [owner, list] : debugUIs_)
	{
		for (auto& ui : list)
		{
			const auto* config = FindWindowConfig(ui.name);
			ui.area = ui.defaultArea;
			ui.visible = config ? config->visible : true;
		}
	}
	showConsole_ = true;
	SaveLayout();
	resetLayoutRequested_ = true;
}

bool DebugUIManager::IsLayoutResetRequested() const
{
	return resetLayoutRequested_;
}

void DebugUIManager::ClearLayoutResetRequest()
{
	resetLayoutRequested_ = false;
}

void DebugUIManager::SetDebugUIArea([[maybe_unused]] void* owner, [[maybe_unused]] DebugUIArea area)
{
	if (owner == nullptr)
	{
		return;
	}

	auto it = debugUIs_.find(owner);
	if (it != debugUIs_.end())
	{
		for (auto& ui : it->second)
		{
			ui.area = area;
		}
		SaveLayout();
	}
}

void DebugUIManager::SetDebugUIArea([[maybe_unused]] const std::string& name, [[maybe_unused]] DebugUIArea area)
{
	if (name.empty())
	{
		return;
	}

	for (auto& [owner, list] : debugUIs_)
	{
		for (auto& ui : list)
		{
			if (ui.name == name)
			{
				ui.area = area;
				SaveLayout();
				return;
			}
		}
	}
}

void DebugUIManager::SetShowConsole(bool show)
{
	if (showConsole_ != show)
	{
		showConsole_ = show;
		SaveLayout();
	}
}

void DebugUIManager::SaveLayout()
{
	// s_savedStatesの更新
	for (const auto& [owner, list] : debugUIs_)
	{
		for (const auto& ui : list)
		{
			s_savedStates[ui.name] = {ui.area, ui.visible};
		}
	}
	s_savedStates["Console"] = {DebugUIArea::Console, showConsole_};

	ImGui::MarkIniSettingsDirty();
	if (ImGui::GetIO().IniFilename != nullptr)
	{
		ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
	}
}

void DebugUIManager::ClearLoadedStates()
{
	s_savedStates.clear();
}

DebugUIManager::SavedUIState& DebugUIManager::GetOrAddLoadedState(const std::string& name)
{
	auto it = s_savedStates.find(name);
	if (it != s_savedStates.end())
	{
		return it->second;
	}

	// デフォルト値
	SavedUIState state;
	state.area = DebugUIArea::Inspector;
	const auto* config = FindWindowConfig(name);
	state.visible = config ? config->visible : true;
	s_savedStates[name] = state;
	return s_savedStates[name];
}

void DebugUIManager::WriteAllSettings(ImGuiTextBuffer* buf)
{
	// インスタンスが存在すればマージ
	if (HasInstance())
	{
		auto* mgr = GetInstance();
		for (const auto& [owner, list] : mgr->debugUIs_)
		{
			for (const auto& ui : list)
			{
				s_savedStates[ui.name] = {ui.area, ui.visible};
			}
		}
		s_savedStates["Console"] = {DebugUIArea::Console, mgr->showConsole_};
	}

	// グローバル設定書き出し
	buf->appendf("[DebugUI][GlobalSettings]\n");
	if (HasInstance())
	{
		buf->appendf("ui_scale=%.2f\n", GetInstance()->GetUIScale());
	}
	else
	{
		buf->appendf("ui_scale=%.2f\n", 1.0f);
	}
	buf->appendf("\n");

	for (const auto& [name, state] : s_savedStates)
	{
		buf->appendf("[DebugUI][%s]\n", name.c_str());
		buf->appendf("area=%d\n", static_cast<int>(state.area));
		buf->appendf("visible=%d\n", state.visible ? 1 : 0);
		buf->appendf("\n");
	}
}

void DebugUIManager::ApplyLoadedStatesToActiveUIs()
{
	for (auto& [owner, list] : debugUIs_)
	{
		for (auto& ui : list)
		{
			auto it = s_savedStates.find(ui.name);
			if (it != s_savedStates.end())
			{
				ui.area = it->second.area;
				ui.visible = it->second.visible;
			}
		}
	}
	auto console = s_savedStates.find("Console");
	if (console != s_savedStates.end())
	{
		showConsole_ = console->second.visible;
	}
}

float DebugUIManager::GetUIScale() const
{
	return uiScale_;
}

void DebugUIManager::SetUIScale(float scale)
{
	if (scale < 0.5f)
	{
		scale = 0.5f;
	}
	if (scale > 3.0f)
	{
		scale = 3.0f;
	}

	if (uiScale_ != scale)
	{
		uiScale_ = scale;
		ApplyUIScale(uiScale_);
		SaveLayout();
	}
}

void DebugUIManager::ApplyUIScale(float scale)
{
	ImGuiIO& io = ImGui::GetIO();
	io.FontGlobalScale = scale;

	float ratio = scale / s_prevScale;
	ImGui::GetStyle().ScaleAllSizes(ratio);
	s_prevScale = scale;
}

#endif // USE_IMGUI
} // namespace KCE
