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
	bool finalVisible = true;

	auto it = s_savedStates.find(name);
	if (it != s_savedStates.end())
	{
		finalArea = it->second.area;
		finalVisible = it->second.visible;
	}

	list.push_back({name, drawFunc, finalArea, finalVisible});
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
	// 何もしない（主要ウィンドウ内の DrawArea で個別に描画されるため）
}

void DebugUIManager::DrawArea([[maybe_unused]] DebugUIArea area)
{
	std::vector<DebugUI*> areaUIs;
	for (auto& [owner, list] : debugUIs_)
	{
		for (auto& ui : list)
		{
			if (ui.area == area && ui.visible)
			{
				areaUIs.push_back(&ui);
			}
		}
	}

	std::sort(areaUIs.begin(), areaUIs.end(), [](const DebugUI* lhs, const DebugUI* rhs) { return lhs->name < rhs->name; });
	for (DebugUI* ui : areaUIs)
	{
		ImGui::PushID(ui);
		ImGuiStorage* storage = ImGui::GetStateStorage();
		const ImGuiID openId = ImGui::GetID("DebugUISectionOpen");
		bool open = storage->GetBool(openId, true);

		// SeparatorText の見た目を保ったまま、行全体を折り畳み操作にする。
		const ImVec2 rowPos = ImGui::GetCursorScreenPos();
		const float rowHeight = ImMax(ImGui::GetTextLineHeightWithSpacing(), 1.0f);
		const float rowWidth = ImMax(ImGui::GetContentRegionAvail().x, 1.0f);
		const ImRect separatorRect(rowPos, ImVec2(rowPos.x + rowWidth, rowPos.y + rowHeight));
		const ImGuiID separatorId = ImGui::GetID("DebugUISeparator");
		ImGui::ItemSize(separatorRect);
		bool separatorHovered = false;
		bool separatorHeld = false;
		if (ImGui::ItemAdd(separatorRect, separatorId) && ImGui::ButtonBehavior(separatorRect, separatorId, &separatorHovered, &separatorHeld))
		{
			open = !open;
			storage->SetBool(openId, open);
		}

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
		const ImU32 separatorColor = ImGui::GetColorU32(ImGuiCol_Separator);
		if (separatorHovered)
		{
			drawList->AddRectFilled(rowPos, ImVec2(rowPos.x + rowWidth, rowPos.y + rowHeight), ImGui::GetColorU32(ImGuiCol_HeaderHovered));
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		}

		const float arrowSize = ImGui::GetFontSize() * 0.70f;
		const ImVec2 arrowPos(rowPos.x + 2.0f, rowPos.y + (rowHeight - arrowSize) * 0.5f);
		ImGui::RenderArrow(drawList, arrowPos, textColor, open ? ImGuiDir_Down : ImGuiDir_Right, 0.70f);
		const ImVec2 textPos(rowPos.x + ImGui::GetFontSize() + 6.0f, rowPos.y + (rowHeight - ImGui::GetFontSize()) * 0.5f);
		drawList->AddText(textPos, textColor, ui->name.c_str());
		const float lineStart = textPos.x + ImGui::CalcTextSize(ui->name.c_str()).x + 10.0f;
		const float lineY = rowPos.y + rowHeight * 0.5f;
		if (lineStart < rowPos.x + rowWidth)
		{
			drawList->AddLine(ImVec2(lineStart, lineY), ImVec2(rowPos.x + rowWidth, lineY), separatorColor);
		}
		if (ImGui::BeginPopupContextItem("DebugUIOptions"))
		{
			const char* areaNames[] = {"Hierarchy", "Inspector", "Console", "Scene", "Project"};
			for (int i = 0; i < IM_ARRAYSIZE(areaNames); ++i)
			{
				if (ImGui::MenuItem(areaNames[i], nullptr, static_cast<int>(ui->area) == i))
				{
					ui->area = static_cast<DebugUIArea>(i);
					SaveLayout();
				}
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Hide"))
			{
				ui->visible = false;
				SaveLayout();
			}
			ImGui::EndPopup();
		}
		if (open && ui->drawFunc)
		{
			ImGui::Indent(4.0f);
			ui->drawFunc();
			ImGui::Unindent(4.0f);
		}
		ImGui::Dummy(ImVec2(0.0f, 4.0f));
		ImGui::PopID();
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

void DebugUIManager::DrawToolsMenu()
{
	// カテゴリ定義
	struct Category
	{
		const char* label;
		const char* names[8];
	};

	static const Category categories[] =
		{
			{"Engine", {"Performance", "Scene Manager", nullptr}},
			{"Time", {"Time Manager", "Timer Manager", nullptr}},
			{"Rendering", {"Light Manager", "Camera Manager", "Particle Manager", nullptr}},
			{"Audio", {"Audio Debug", nullptr}},
			{"Editor Tools", {"JSON Editor", "Font Sprite", nullptr}},
			{"Effects", {"Scene Transition", nullptr}},
			{"Scenes", {"Title Scene", nullptr}},
		};

	for (const auto& cat : categories)
	{
		// カテゴリ内の登録有無チェック
		bool hasItem = false;
		for (const auto& [owner, list] : debugUIs_)
		{
			for (const auto& ui : list)
			{
				for (int ni = 0; cat.names[ni] != nullptr; ++ni)
				{
					if (ui.name == cat.names[ni])
					{
						hasItem = true;
						break;
					}
				}
				if (hasItem)
				{
					break;
				}
			}
			if (hasItem)
			{
				break;
			}
		}

		if (!hasItem)
		{
			continue;
		}

		if (ImGui::BeginMenu(cat.label))
		{
			for (auto& [owner, list] : debugUIs_)
			{
				for (auto& ui : list)
				{
					for (int ni = 0; cat.names[ni] != nullptr; ++ni)
					{
						if (ui.name == cat.names[ni])
						{
							if (ImGui::MenuItem(ui.name.c_str(), nullptr, &ui.visible))
							{
								SaveLayout();
							}
							break;
						}
					}
				}
			}
			ImGui::EndMenu();
		}
	}

	// 未分類UIの収集
	std::vector<DebugUI*> otherUIs;
	for (auto& [owner, list] : debugUIs_)
	{
		for (auto& ui : list)
		{
			bool classified = false;
			for (const auto& cat : categories)
			{
				for (int ni = 0; cat.names[ni] != nullptr; ++ni)
				{
					if (ui.name == cat.names[ni])
					{
						classified = true;
						break;
					}
				}
				if (classified)
				{
					break;
				}
			}

			if (!classified)
			{
				otherUIs.push_back(&ui);
			}
		}
	}

	if (!otherUIs.empty())
	{
		if (ImGui::BeginMenu("Others"))
		{
			for (auto* uiPtr : otherUIs)
			{
				if (ImGui::MenuItem(uiPtr->name.c_str(), nullptr, &uiPtr->visible))
				{
					SaveLayout();
				}
			}
			ImGui::EndMenu();
		}
	}

	ImGui::Separator();

	// エリア変更用サブメニュー
	if (ImGui::BeginMenu("UI Area Settings"))
	{
		for (auto& [owner, list] : debugUIs_)
		{
			for (auto& ui : list)
			{
				if (ImGui::BeginMenu(ui.name.c_str()))
				{
					int currentArea = static_cast<int>(ui.area);
					const char* areaNames[] = {"Hierarchy", "Inspector", "Console", "Scene", "Project"};
					for (int i = 0; i < 5; ++i)
					{
						bool selected = (currentArea == i);
						if (ImGui::MenuItem(areaNames[i], nullptr, &selected))
						{
							ui.area = static_cast<DebugUIArea>(i);
							SaveLayout();
						}
					}
					ImGui::EndMenu();
				}
			}
		}
		ImGui::EndMenu();
	}
}

void DebugUIManager::RequestLayoutReset()
{
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
	state.visible = true;
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
