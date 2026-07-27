#include "GameObjectEditor.h"
#include "engine/gameobject/base/GameObject.h"
#include "engine/gameobject/manager/GameObjectManager.h"
#include "manager/editor/DebugUIManager.h"
#include "externals/imgui/imgui.h"
#include <filesystem>
#include <fstream>

// Factory
#include "engine/gameobject/component/base/ComponentFactory.h"

namespace KCE
{
std::unique_ptr<GameObjectEditor> GameObjectEditor::instance_ = nullptr;

GameObjectEditor* GameObjectEditor::GetInstance()
{
	if (!instance_)
	{
		instance_ = std::make_unique<GameObjectEditor>();
	}
	return instance_.get();
}

bool GameObjectEditor::HasInstance()
{
	return instance_ != nullptr;
}

void GameObjectEditor::Initialize()
{
	selected_ = nullptr;

	// ファクトリに登録されているすべてのコンポーネント名を取得
	availableComponents_ = GameObjectComponent::ComponentFactory::GetInstance()->GetRegisteredNames();

	selectedCompIndex_ = 0;
	selectedJsonIndex_ = 0;

	// 既存JSONファイル一覧の取得
	UpdateJsonFileList();

#ifdef USE_IMGUI
	// DebugUIManager に登録
	DebugUIManager::GetInstance()->RegisterDebugUI(
		this,
		"GameObject Editor",
		[this]() { this->DrawImGui(); },
		DebugUIArea::Project
	);
#endif
}

void GameObjectEditor::Finalize()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->UnregisterDebugUI(this);
	}
#endif
	selected_ = nullptr;
	instance_.reset();
}

void GameObjectEditor::OnGameObjectRemoved(GameObject* gameObject)
{
	if (selected_ == gameObject)
	{
		selected_ = nullptr;
	}
}

void GameObjectEditor::DrawImGui()
{
#ifdef USE_IMGUI
	// ------------------ 上部: GameObject List ------------------
	ImGui::SeparatorText("GameObject List");

	if (ImGui::Button("Create GameObject", ImVec2(-FLT_MIN, 0.0f)))
	{
		GameObject* newObj = GameObjectManager::GetInstance()->CreateGameObject("NewObject", "GameObject");
		if (newObj)
		{
			selected_ = newObj;
		}
	}

	ImGui::Spacing();

	// 階層リストの高さ（全体の邪魔にならないよう適度な固定高にする）
	ImGui::BeginChild("HierarchyList", ImVec2(0, 150.0f), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
	const auto& objects = GameObjectManager::GetInstance()->GetGameObjects();
	for (GameObject* obj : objects)
	{
		if (!obj) continue;

		std::string label = obj->GetName() + " (" + obj->GetTag() + ")";
		bool isSelected = (selected_ == obj);
		if (ImGui::Selectable(label.c_str(), isSelected))
		{
			selected_ = obj;
		}
	}
	ImGui::EndChild();

	if (selected_)
	{
		if (ImGui::Button("Delete Selected", ImVec2(-FLT_MIN, 0.0f)))
		{
			// 不正ポインタ参照を防ぐため、Unregister前にselected_をクリア
			GameObject* objToDelete = selected_;
			selected_ = nullptr;
			GameObjectManager::GetInstance()->Unregister(objToDelete);
		}
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// ------------------ 下部: GameObject Details (プロパティ/コンポーネント) ------------------
	ImGui::SeparatorText("GameObject Details");

	// スクロール可能な詳細領域（残り領域を全て使い、はみ出た場合はスクロールさせる）
	ImGui::BeginChild("InspectorDetailsArea", ImVec2(0, -75.0f), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

	if (selected_)
	{
		// 1. JSON ファイル操作（折りたたみ可能にしてスッキリさせる）
		if (ImGui::CollapsingHeader("JSON Storage", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Spacing();
			ImGui::PushItemWidth(200.0f);
			if (ImGui::InputText("JSON File Name", fileNameBuf_, sizeof(fileNameBuf_)))
			{
				// 入力時にもファイル一覧をリフレッシュ
			}
			ImGui::PopItemWidth();

			if (ImGui::Button("Save JSON", ImVec2(100.0f, 0.0f)))
			{
				if (selected_->SaveJson(fileNameBuf_))
				{
					UpdateJsonFileList(); // セーブ成功したら一覧を更新
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Load (Overwrite)", ImVec2(130.0f, 0.0f)))
			{
				selected_->LoadJson(fileNameBuf_);
			}
			ImGui::Spacing();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// 2. 基本プロパティ＆アタッチされたコンポーネントのパラメータ描画
		selected_->DrawImGui();

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// 3. コンポーネントの管理（追加・削除を一目でわかりやすく整理）
		if (ImGui::CollapsingHeader("Components Manager", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Spacing();
			ImGui::Text("Attached Components:");
			ImGui::Indent();

			std::vector<std::string> toRemove;
			const auto& comps = selected_->GetComponents();
			if (comps.empty())
			{
				ImGui::TextDisabled("No components attached.");
			}
			else
			{
				for (const auto& [compName, comp] : comps)
				{
					ImGui::PushID(compName.c_str());
					ImGui::AlignTextToFramePadding();
					ImGui::Bullet();
					ImGui::Text("%s", compName.c_str());
					ImGui::SameLine(ImGui::GetWindowWidth() - 110.0f);
					if (ImGui::Button("Remove", ImVec2(75.0f, 0.0f)))
					{
						toRemove.push_back(compName);
					}
					ImGui::PopID();
				}
			}

			for (const auto& compName : toRemove)
			{
				selected_->RemoveComponent(compName);
			}

			ImGui::Unindent();

			ImGui::Spacing();

			// コンポーネント追加セクション
			if (!availableComponents_.empty())
			{
				ImGui::Text("Add New Component:");
				ImGui::PushItemWidth(180.0f);
				if (ImGui::BeginCombo("##CompSelect", availableComponents_[selectedCompIndex_].c_str()))
				{
					for (int i = 0; i < static_cast<int>(availableComponents_.size()); ++i)
					{
						bool isSel = (selectedCompIndex_ == i);
						if (ImGui::Selectable(availableComponents_[i].c_str(), isSel))
						{
							selectedCompIndex_ = i;
						}
					}
					ImGui::EndCombo();
				}
				ImGui::PopItemWidth();
				ImGui::SameLine();
				if (ImGui::Button("Add Component"))
				{
					AddComponentByName(selected_, availableComponents_[selectedCompIndex_]);
				}
			}
			ImGui::Spacing();
		}
	}
	else
	{
		ImGui::TextDisabled("Select a GameObject from the list to edit properties.");
	}

	ImGui::EndChild(); // InspectorDetailsArea

	// ------------------ 最下部: 既存 JSON のシーンロード ------------------
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();
	ImGui::Text("Load Existing JSON:");

	if (!jsonFiles_.empty())
	{
		ImGui::PushItemWidth(180.0f);
		if (ImGui::BeginCombo("##JsonListCombo", jsonFiles_[selectedJsonIndex_].c_str()))
		{
			for (int i = 0; i < static_cast<int>(jsonFiles_.size()); ++i)
			{
				bool isSel = (selectedJsonIndex_ == i);
				if (ImGui::Selectable(jsonFiles_[i].c_str(), isSel))
				{
					selectedJsonIndex_ = i;
					strncpy_s(fileNameBuf_, jsonFiles_[i].c_str(), sizeof(fileNameBuf_));
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
		ImGui::SameLine();

		if (ImGui::Button("Load As New Object"))
		{
			std::string fileToLoad = jsonFiles_[selectedJsonIndex_];
			std::string fullPath = "Resources/json/gameobject/" + fileToLoad;
			std::ifstream ifs(fullPath);
			if (ifs)
			{
				nlohmann::json json;
				try
				{
					ifs >> json;
					ifs.close();

					std::string rawName = std::filesystem::path(fileToLoad).stem().string();
					GameObject* newObj = GameObjectManager::GetInstance()->CreateGameObject(rawName, "GameObject");
					if (newObj)
					{
						if (json.contains("components") && json["components"].is_object())
						{
							for (auto it = json["components"].begin(); it != json["components"].end(); ++it)
							{
								std::string compName = it.key();
								auto compInstance = GameObjectComponent::ComponentFactory::GetInstance()->Create(compName, newObj);
								if (compInstance)
								{
									newObj->AddComponent(compName, std::move(compInstance));
								}
							}
						}

						if (newObj->LoadJson(fileToLoad))
						{
							selected_ = newObj;
						}
						else
						{
							GameObjectManager::GetInstance()->Unregister(newObj);
						}
					}
				}
				catch (...)
				{
					ifs.close();
				}
			}
		}
	}
	else
	{
		ImGui::TextDisabled("No JSON files found.");
	}

	ImGui::SameLine();
	if (ImGui::Button("Refresh List"))
	{
		UpdateJsonFileList();
	}
#endif
}

void GameObjectEditor::AddComponentByName(GameObject* owner, const std::string& compTypeName)
{
	if (!owner) return;

	auto comp = GameObjectComponent::ComponentFactory::GetInstance()->Create(compTypeName, owner);
	if (comp)
	{
		owner->AddComponent(compTypeName, std::move(comp));
	}
}

void GameObjectEditor::UpdateJsonFileList()
{
	jsonFiles_.clear();
	std::string dirPath = "Resources/json/gameobject/";
	try
	{
		std::filesystem::create_directories(dirPath);
		if (std::filesystem::exists(dirPath))
		{
			for (const auto& entry : std::filesystem::directory_iterator(dirPath))
			{
				if (entry.is_regular_file() && entry.path().extension() == ".json")
				{
					jsonFiles_.push_back(entry.path().filename().string());
				}
			}
		}
	}
	catch (...) {}

	// インデックスがファイル数を超えないように調整
	if (selectedJsonIndex_ >= static_cast<int>(jsonFiles_.size()))
	{
		selectedJsonIndex_ = 0;
	}
}
} // namespace KCE
