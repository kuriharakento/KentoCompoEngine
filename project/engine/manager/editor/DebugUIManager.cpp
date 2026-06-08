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
	bool isFirst = true;
	for (auto& [owner, list] : debugUIs_)
	{
		for (auto& ui : list)
		{
			// エリアが違う、または非表示ならスキップ
			if (ui.area != area || !ui.visible)
			{
				continue;
			}

			if (!isFirst)
			{
				// アコーディオン同士の間に余白と境界線を入れる
				ImGui::Dummy(ImVec2(0.0f, 4.0f));
				ImGui::Separator();
				ImGui::Dummy(ImVec2(0.0f, 4.0f));
			}
			isFirst = false;

			// 折りたたみヘッダーとして描画
			if (ImGui::CollapsingHeader(ui.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Indent(10.0f);
				ImGui::Spacing();

				// 表示エリアの動的変更UI
				int currentArea = static_cast<int>(ui.area);
				const char* areaNames[] = { "Hierarchy", "Inspector", "Console", "Scene", "Project" };
				ImGui::TextDisabled("Move to:");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(100.0f);
				std::string comboId = "##MoveArea_" + ui.name;
				if (ImGui::Combo(comboId.c_str(), &currentArea, areaNames, IM_ARRAYSIZE(areaNames)))
				{
					ui.area = static_cast<DebugUIArea>(currentArea);
				}
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				if (ui.drawFunc)
				{
					ui.drawFunc();
				}
				ImGui::Spacing();
				ImGui::Unindent(10.0f);
			}
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
