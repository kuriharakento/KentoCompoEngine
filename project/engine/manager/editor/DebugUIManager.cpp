#include "DebugUIManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_internal.h"
#include "externals/nlohmann/json.hpp"
#include <fstream>
#endif

#ifdef USE_IMGUI
static std::unordered_map<std::string, DebugUIManager::SavedUIState> s_savedStates;
static float s_prevScale = 1.0f;
#endif

std::unique_ptr<DebugUIManager> DebugUIManager::instance_ = nullptr;

DebugUIManager* DebugUIManager::GetInstance()
{
	if (instance_ == nullptr)
	{
		instance_.reset(new DebugUIManager());
	}
	return instance_.get();
}

bool DebugUIManager::HasInstance()
{
	return instance_ != nullptr;
}

void DebugUIManager::Initialize()
{
#ifdef USE_IMGUI
	debugUIs_.clear();

	// 旧ファイルの削除（互換性のクリーンアップ）
	std::remove("debug_ui_layout.json");

	// ImGuiのカスタム設定ハンドラーの登録
	ImGuiContext* ctx = ImGui::GetCurrentContext();
	if (ctx != nullptr)
	{
		ImGuiSettingsHandler ini_handler;
		ini_handler.TypeName = "DebugUI";
		ini_handler.TypeHash = ImHashStr("DebugUI");

		ini_handler.ClearAllFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler) {
			DebugUIManager::GetInstance()->ClearLoadedStates();
		};
		ini_handler.ReadOpenFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name) -> void* {
			if (strcmp(name, "GlobalSettings") == 0)
			{
				return (void*)DebugUIManager::GetInstance();
			}
			return (void*)&(DebugUIManager::GetInstance()->GetOrAddLoadedState(name));
		};
		ini_handler.ReadLineFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line) {
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
		ini_handler.ApplyAllFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler) {
			DebugUIManager::GetInstance()->ApplyLoadedStatesToActiveUIs();
		};
		ini_handler.WriteAllFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf) {
			DebugUIManager::GetInstance()->WriteAllSettings(buf);
		};

		if (ImGui::FindSettingsHandler("DebugUI") == nullptr)
		{
			ImGui::AddSettingsHandler(&ini_handler);
		}
	}
#endif
	resetLayoutRequested_ = false;

	uiScale_ = 1.0f;
	s_prevScale = 1.0f;

	showHierarchy_ = true;
	showInspector_ = true;
	showConsole_   = true;
	showProject_   = true;
}

void DebugUIManager::Finalize()
{
#ifdef USE_IMGUI
	debugUIs_.clear();
#endif
	instance_.reset();
}

void DebugUIManager::RegisterDebugUI([[maybe_unused]] void* owner, [[maybe_unused]] const std::string& name, [[maybe_unused]] std::function<void()> drawFunc, [[maybe_unused]] DebugUIArea area)
{
#ifdef USE_IMGUI
	if (owner == nullptr || name.empty() || !drawFunc)
	{
		return;
	}

	// 同じ owner で同名が既に登録されていたら上書き
	auto& list = debugUIs_[owner];
	for (auto& ui : list)
	{
		if (ui.name == name)
		{
			ui.drawFunc = drawFunc;
			// エリア変更等は実行時変更が優先されるため上書きしない
			return;
		}
	}

	// 新規登録（ロード済みの状態があれば適用、なければデフォルト値）
	DebugUIArea finalArea = area;
	bool finalVisible = true;

	auto it = s_savedStates.find(name);
	if (it != s_savedStates.end())
	{
		finalArea = it->second.area;
		finalVisible = it->second.visible;
	}

	list.push_back({ name, drawFunc, finalArea, finalVisible });
#endif
}

void DebugUIManager::UnregisterDebugUI([[maybe_unused]] void* owner)
{
#ifdef USE_IMGUI
	if (owner == nullptr)
	{
		return;
	}

	debugUIs_.erase(owner);
#endif
}

void DebugUIManager::Clear()
{
#ifdef USE_IMGUI
	debugUIs_.clear();
#endif
}

void DebugUIManager::Draw()
{
	// 何もしない（主要ウィンドウ内の DrawArea で個別に描画されるため）
}
void DebugUIManager::DrawArea([[maybe_unused]] DebugUIArea area)
{
#ifdef USE_IMGUI
	for (auto& [owner, list] : debugUIs_)
	{
		for (auto& ui : list)
		{
			// エリアが違う、または非表示ならスキップ
			if (ui.area != area || !ui.visible)
			{
				continue;
			}

			ImGui::PushID(ui.name.c_str());

			// カードごとの背景色を計算
			ImVec4 window_bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
			ImVec4 card_bg = window_bg;
			card_bg.x += 0.05f;
			card_bg.y += 0.05f;
			card_bg.z += 0.05f;
			if (card_bg.x > 1.0f)
			{
				card_bg.x = 1.0f;
			}
			if (card_bg.y > 1.0f)
			{
				card_bg.y = 1.0f;
			}
			if (card_bg.z > 1.0f)
			{
				card_bg.z = 1.0f;
			}

			ImGui::PushStyleColor(ImGuiCol_ChildBg, card_bg);
			ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

			// デフォルトサイズを指定（リサイズされていれば imgui.ini の設定が優先される）
			ImVec2 child_size = ImVec2(0.0f, 180.0f);
			ImGuiChildFlags child_flags = ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY;

			if (ImGui::BeginChild(ui.name.c_str(), child_size, child_flags, 0))
			{
				// ヘッダー専用の非常にコンパクトなスタイルを適用
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 1.0f));
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 2.0f));

				// カード上部とテキストの間のわずかな隙間
				ImGui::Dummy(ImVec2(0.0f, 2.0f));

				// 左右のパディング（左端から8px）
				ImGui::Indent(8.0f);

				// テキストのベースラインを揃える
				ImGui::AlignTextToFramePadding();
				ImGui::Text(ui.name.c_str());

				// 右寄せの Move と 閉じるボタン（左右の幅は余裕を持たせる）
				float avail_width = ImGui::GetContentRegionAvail().x;
				float combo_width = 90.0f; // 余裕を持たせた幅
				float text_width = ImGui::CalcTextSize("Move:").x;
				float close_btn_width = 20.0f; // 余裕を持たせた幅
				float space_needed = combo_width + text_width + close_btn_width + 16.0f; // コントロール間の間隔も広めに
				float right_align_x = avail_width - space_needed - 8.0f; // 右端からも 8px 空ける
				if (right_align_x > 40.0f)
				{
					ImGui::SameLine(ImGui::GetCursorPosX() + right_align_x);
				}
				else
				{
					ImGui::SameLine();
				}

				ImGui::TextDisabled("Move:");
				ImGui::SameLine(0.0f, 6.0f); // 余裕を持たせる
				ImGui::SetNextItemWidth(combo_width);
				int currentArea = static_cast<int>(ui.area);
				const char* areaNames[] = { "Hierarchy", "Inspector", "Console", "Scene", "Project" };
				std::string comboId = "##MoveArea_" + ui.name;

				if (ImGui::Combo(comboId.c_str(), &currentArea, areaNames, IM_ARRAYSIZE(areaNames)))
				{
					ui.area = static_cast<DebugUIArea>(currentArea);
					SaveLayout();
				}

				// 閉じる [X] ボタンを描画（左右の間隔を広げる）
				ImGui::SameLine(0.0f, 10.0f);
				std::string closeButtonId = "x##Close_" + ui.name;
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.2f, 0.2f, 0.6f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.1f, 0.1f, 0.9f));
				if (ImGui::Button(closeButtonId.c_str(), ImVec2(close_btn_width, 18.0f)))
				{
					ui.visible = false;
					SaveLayout();
				}
				ImGui::PopStyleColor(3);

				// セパレーターを描画（余白を詰めつつ引く）
				ImGui::Dummy(ImVec2(0.0f, 2.0f));
				ImGui::Separator();
				ImGui::Dummy(ImVec2(0.0f, 4.0f));

				// ヘッダー専用のスタイルを復元（これにより drawFunc の中身は通常サイズで描画される）
				ImGui::PopStyleVar(2);

				// カスタム描画関数の実行
				if (ui.drawFunc)
				{
					ui.drawFunc();
				}

				// カード下部のパディング
				ImGui::Dummy(ImVec2(0.0f, 4.0f));

				ImGui::Unindent(8.0f);
			}
			ImGui::EndChild();

			ImGui::PopStyleVar(2); // WindowPadding, ChildRounding
			ImGui::PopStyleColor();

			// カード同士の間のスペース
			ImGui::Dummy(ImVec2(0.0f, 8.0f));

			ImGui::PopID();
		}
	}
#endif
}

void DebugUIManager::DrawToolsMenu()
{
#ifdef USE_IMGUI
	// カテゴリ名と、そのカテゴリに属する UI 名の対応表
	struct Category
	{
		const char* label;
		const char* names[8]; // 各カテゴリに含まれる UI 名（nullptr 終端）
	};

	static const Category categories[] =
	{
		{ "Engine",       { "Performance", "Scene Manager", nullptr } },
		{ "Time",         { "Time Manager", "Timer Manager", nullptr } },
		{ "Rendering",    { "Light Manager", "Camera Manager", "Particle Manager", nullptr } },
		{ "Audio",        { "Audio Debug", nullptr } },
		{ "Editor Tools", { "JSON Editor", "Font Sprite", nullptr } },
		{ "Effects",      { "Scene Transition", nullptr } },
		{ "Scenes",       { "Title Scene", nullptr } },
	};

	for (const auto& cat : categories)
	{
		// このカテゴリに属する登録済み UI が 1 つでもあるか確認
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
				if (hasItem) { break; }
			}
			if (hasItem) { break; }
		}

		if (!hasItem) { continue; }

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

	// カテゴリ未分類の登録済み UI を集めて表示
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
				if (classified) { break; }
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

	// UIごとの表示エリア変更用サブメニュー
	if (ImGui::BeginMenu("UI Area Settings"))
	{
		for (auto& [owner, list] : debugUIs_)
		{
			for (auto& ui : list)
			{
				if (ImGui::BeginMenu(ui.name.c_str()))
				{
					int currentArea = static_cast<int>(ui.area);
					const char* areaNames[] = { "Hierarchy", "Inspector", "Console", "Scene", "Project" };
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
#endif
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
#ifdef USE_IMGUI
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
#endif
}

void DebugUIManager::SetDebugUIArea([[maybe_unused]] const std::string& name, [[maybe_unused]] DebugUIArea area)
{
#ifdef USE_IMGUI
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
#endif
}

void DebugUIManager::SaveLayout()
{
#ifdef USE_IMGUI
	// 現在の登録UIの最新状態でs_savedStatesを更新
	for (const auto& [owner, list] : debugUIs_)
	{
		for (const auto& ui : list)
		{
			s_savedStates[ui.name] = { ui.area, ui.visible };
		}
	}

	ImGui::MarkIniSettingsDirty();
	if (ImGui::GetIO().IniFilename != nullptr)
	{
		ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
	}
#endif
}

void DebugUIManager::ClearLoadedStates()
{
#ifdef USE_IMGUI
	s_savedStates.clear();
#endif
}

DebugUIManager::SavedUIState& DebugUIManager::GetOrAddLoadedState(const std::string& name)
{
#ifdef USE_IMGUI
	auto it = s_savedStates.find(name);
	if (it != s_savedStates.end())
	{
		return it->second;
	}

	// 新規登録時のデフォルト設定
	SavedUIState state;
	state.area = DebugUIArea::Inspector;
	state.visible = true;
	s_savedStates[name] = state;
	return s_savedStates[name];
#else
	static SavedUIState dummy;
	return dummy;
#endif
}

void DebugUIManager::WriteAllSettings(ImGuiTextBuffer* buf)
{
#ifdef USE_IMGUI
	// シングルトンが生きていれば最新状態をマージ
	if (HasInstance())
	{
		auto* mgr = GetInstance();
		for (const auto& [owner, list] : mgr->debugUIs_)
		{
			for (const auto& ui : list)
			{
				s_savedStates[ui.name] = { ui.area, ui.visible };
			}
		}
	}

	// グローバル設定の書き出し
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
#endif
}

void DebugUIManager::ApplyLoadedStatesToActiveUIs()
{
#ifdef USE_IMGUI
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
#endif
}

float DebugUIManager::GetUIScale() const
{
	return uiScale_;
}

void DebugUIManager::SetUIScale(float scale)
{
#ifdef USE_IMGUI
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
#else
	uiScale_ = scale;
#endif
}

void DebugUIManager::ApplyUIScale(float scale)
{
#ifdef USE_IMGUI
	ImGuiIO& io = ImGui::GetIO();
	io.FontGlobalScale = scale;

	float ratio = scale / s_prevScale;
	ImGui::GetStyle().ScaleAllSizes(ratio);
	s_prevScale = scale;
#else
	(void)scale;
#endif
}
