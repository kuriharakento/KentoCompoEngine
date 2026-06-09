#include "DebugUIManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_internal.h"
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
#endif
	resetLayoutRequested_ = false;

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
			ui.area     = area;
			return;
		}
	}

	// 新規登録（visible はデフォルト true）
	list.push_back({ name, drawFunc, area, true });
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

			float card_x = ImGui::GetCursorScreenPos().x;
			float card_width = ImGui::GetContentRegionAvail().x;

			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			draw_list->ChannelsSplit(2);
			draw_list->ChannelsSetCurrent(1); // 前面（コンテンツ）

			ImGui::BeginGroup();
			
			// カード上部の内部パディング
			ImGui::Dummy(ImVec2(0.0f, 6.0f));

			// 左右の内部パディング（インデント）
			ImGui::Indent(8.0f);

			// タイトル（太字や特別な装飾はなくシンプルに表示）
			ImGui::Text(ui.name.c_str());

			// 「Move to:」コンボボックスをヘッダー右端に配置
			float combo_width = 85.0f;
			float text_width = ImGui::CalcTextSize("Move:").x;
			float space_needed = combo_width + text_width + 4.0f;
			float right_align_x = card_width - 16.0f - space_needed;
			if (right_align_x > 100.0f)
			{
				ImGui::SameLine(right_align_x + 8.0f); // インデント分調整して右寄せ
			}
			else
			{
				ImGui::SameLine();
			}

			ImGui::TextDisabled("Move:");
			ImGui::SameLine(0.0f, 4.0f);
			ImGui::SetNextItemWidth(combo_width);
			int currentArea = static_cast<int>(ui.area);
			const char* areaNames[] = { "Hierarchy", "Inspector", "Console", "Scene", "Project" };
			std::string comboId = "##MoveArea_" + ui.name;
			if (ImGui::Combo(comboId.c_str(), &currentArea, areaNames, IM_ARRAYSIZE(areaNames)))
			{
				ui.area = static_cast<DebugUIArea>(currentArea);
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// カスタム描画関数の実行
			if (ui.drawFunc)
			{
				ui.drawFunc();
			}

			// カード下部の内部パディング
			ImGui::Dummy(ImVec2(0.0f, 6.0f));

			ImGui::Unindent(8.0f);
			ImGui::EndGroup();

			// グループの描画領域を取得
			ImVec2 p_min = ImGui::GetItemRectMin();
			ImVec2 p_max = ImGui::GetItemRectMax();

			// 左右の幅をカードのコンテンツ領域幅に合わせる
			p_min.x = card_x;
			p_max.x = card_x + card_width;

			// 背面（背景と枠線）を描画
			draw_list->ChannelsSetCurrent(0);

			ImVec4 window_bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
			// ウィンドウの背景色より少し明るい色にして視覚的に分離する
			ImVec4 card_bg = window_bg;
			card_bg.x += 0.05f;
			card_bg.y += 0.05f;
			card_bg.z += 0.05f;
			if (card_bg.x > 1.0f) card_bg.x = 1.0f;
			if (card_bg.y > 1.0f) card_bg.y = 1.0f;
			if (card_bg.z > 1.0f) card_bg.z = 1.0f;

			ImU32 card_bg_u32 = ImGui::ColorConvertFloat4ToU32(card_bg);
			ImU32 border_col = ImGui::GetColorU32(ImGuiCol_Border);

			draw_list->AddRectFilled(p_min, p_max, card_bg_u32, 4.0f);
			draw_list->AddRect(p_min, p_max, border_col, 4.0f);

			draw_list->ChannelsMerge();

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
							ImGui::MenuItem(ui.name.c_str(), nullptr, &ui.visible);
							break;
						}
					}
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
				return;
			}
		}
	}
#endif
}
