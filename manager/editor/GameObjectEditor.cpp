#include "GameObjectEditor.h"
#include "base/PathManager.h"
#include "engine/gameobject/base/GameObject.h"
#include "engine/gameobject/manager/GameObjectManager.h"
#include "manager/editor/DebugUIManager.h"
#include "editor/SelectionContext.h"
#include "editor/SceneGizmo.h"
#include "editor/command/CommandHistory.h"
#include "sequencer/editor/SequencerCommands.h"
#include "externals/imgui/imgui.h"
#include <algorithm>
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
	selectedPrefabIndex_ = 0;

	// 既存JSONファイル一覧の取得
	UpdateJsonFileList();
	UpdatePrefabFileList();

#ifdef USE_IMGUI
	// 一覧は Hierarchy の区画、詳細は Inspector に出す
	DebugUIManager::GetInstance()->RegisterHierarchySection(
		this,
		"GameObject",
		[this]() { this->DrawListImGui(); }
	);
	DebugUIManager::GetInstance()->RegisterInspector(
		this,
		SelectionKind::GameObject,
		[this](const SelectionItem&)
		{
			// Hierarchy 以外（ギズモなど）で選ばれても中身がずれないよう、選択の一元管理に合わせてから描く
			selected_ = SelectionContext::GetInstance()->GetPrimaryGameObject();
			this->DrawInspectorImGui();
		}
	);

	// 選んでいる GameObject をギズモで動かす。対象は毎回選択から引き直す（消えたオブジェクトを掴まないため）
	GizmoTarget objectTarget;
	objectTarget.getPose = [](const SelectionItem&, Matrix4x4& world, uint32_t& operations)
	{
		GameObject* object = SelectionContext::GetInstance()->GetPrimaryGameObject();
		if (!object)
		{
			return false;
		}
		world = MakeAffineMatrix(object->GetScale(), object->GetRotation(), object->GetPosition());
		operations = kGizmoAll;
		return true;
	};
	objectTarget.apply = [](const SelectionItem&, const GizmoResult& after, uint32_t dragId)
	{
		GameObject* object = SelectionContext::GetInstance()->GetPrimaryGameObject();
		if (!object)
		{
			return;
		}
		Transform before;
		before.scale = object->GetScale();
		before.rotate = object->GetRotation();
		before.translate = object->GetPosition();
		// 回転はエンジンのオイラー角の規約にそろえる
		Transform moved;
		moved.scale = after.scale;
		moved.rotate = after.rotate.ToEuler();
		moved.translate = after.translate;
		CommandHistory::GetInstance()->Execute(std::make_unique<GameObjectTransformCommand>(object->GetGuid(), before, moved, dragId));
	};
	SceneGizmo::GetInstance()->RegisterTarget(this, SelectionKind::GameObject, std::move(objectTarget));
#endif
}

void GameObjectEditor::Finalize()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->Unregister(this);
	}
	// SceneGizmo は先に片付いていることがある
	if (SceneGizmo::HasInstance())
	{
		SceneGizmo::GetInstance()->Unregister(this);
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

void GameObjectEditor::DrawListImGui()
{
#ifdef USE_IMGUI
	if (ImGui::Button("GameObjectを作成", ImVec2(-FLT_MIN, 0.0f)))
	{
		GameObject* newObj = GameObjectManager::GetInstance()->CreateGameObject("NewObject", "GameObject");
		if (newObj)
		{
			selected_ = newObj;
			// シーン上のギズモは選択の一元管理（SelectionContext）を見るので、そちらにも伝える
			SelectionContext::GetInstance()->SelectGameObject(newObj);
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
			// シーン上のギズモは選択の一元管理（SelectionContext）を見るので、そちらにも伝える
			SelectionContext::GetInstance()->SelectGameObject(obj);
		}
	}
	ImGui::EndChild();

	if (selected_)
	{
		if (ImGui::Button("選択中を削除", ImVec2(-FLT_MIN, 0.0f)))
		{
			// 不正ポインタ参照を防ぐため、Unregister前にselected_をクリア
			GameObject* objToDelete = selected_;
			selected_ = nullptr;
			GameObjectManager::GetInstance()->Unregister(objToDelete);
		}
	}

#endif
}

void GameObjectEditor::DrawInspectorImGui()
{
#ifdef USE_IMGUI

	// スクロール可能な詳細領域（残り領域を全て使い、はみ出た場合はスクロールさせる）
	ImGui::BeginChild("InspectorDetailsArea", ImVec2(0, -75.0f), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

	if (selected_)
	{
		// 1. JSON ファイル操作（折りたたみ可能にしてスッキリさせる）
		if (ImGui::CollapsingHeader("JSON保存", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Spacing();
			ImGui::PushItemWidth(200.0f);
			if (ImGui::InputText("JSONファイル名", fileNameBuf_, sizeof(fileNameBuf_)))
			{
				// 入力時にもファイル一覧をリフレッシュ
			}
			ImGui::PopItemWidth();

			if (ImGui::Button("JSONを保存", ImVec2(100.0f, 0.0f)))
			{
				if (selected_->SaveJson(fileNameBuf_))
				{
					UpdateJsonFileList(); // セーブ成功したら一覧を更新
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("読み込み（上書き）", ImVec2(130.0f, 0.0f)))
			{
				selected_->LoadJson(fileNameBuf_);
			}
			ImGui::Spacing();
		}

		if (ImGui::CollapsingHeader("プレハブ", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PushItemWidth(200.0f);
			ImGui::InputText("プレハブファイル名", prefabFileNameBuf_, sizeof(prefabFileNameBuf_));
			ImGui::PopItemWidth();
			if (ImGui::Button("プレハブとして保存"))
			{
				if (selected_->SavePrefab(prefabFileNameBuf_))
				{
					UpdatePrefabFileList();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("プレハブを生成") && !prefabFiles_.empty())
			{
				GameObject* object = GameObjectManager::GetInstance()->Instantiate(prefabFiles_[selectedPrefabIndex_]);
				if (object)
				{
					selected_ = object;
					SelectionContext::GetInstance()->SelectGameObject(object);
				}
			}
			if (!prefabFiles_.empty())
			{
				ImGui::PushItemWidth(200.0f);
				if (ImGui::BeginCombo("プレハブ元", prefabFiles_[selectedPrefabIndex_].c_str()))
				{
					for (int index = 0; index < static_cast<int>(prefabFiles_.size()); ++index)
					{
						if (ImGui::Selectable(prefabFiles_[index].c_str(), selectedPrefabIndex_ == index))
						{
							selectedPrefabIndex_ = index;
						}
					}
					ImGui::EndCombo();
				}
				ImGui::PopItemWidth();
			}
			else
			{
				ImGui::TextDisabled("プレハブファイルがありません。");
			}
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
		if (ImGui::CollapsingHeader("Componentマネージャー", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Spacing();
			ImGui::Text("追加済みComponent:");
			ImGui::Indent();

			std::vector<std::string> toRemove;
			const auto& comps = selected_->GetComponents();
			if (comps.empty())
			{
				ImGui::TextDisabled("Componentはありません。");
			}
			else
			{
				const auto& componentNames = selected_->GetComponentTypeNames();
				for (size_t index = 0; index < comps.size(); ++index)
				{
					const auto& compName = componentNames[index];
					const auto& comp = comps[index];
					// 同じ種類を2つ持てるので、種類名ではなく並び順で ID を分ける
					ImGui::PushID(static_cast<int>(index));
					ImGui::AlignTextToFramePadding();
					ImGui::Bullet();
					ImGui::Text("%s", compName.c_str());
					ImGui::SameLine(ImGui::GetWindowWidth() - 110.0f);
					if (ImGui::Button("削除", ImVec2(75.0f, 0.0f)))
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
				ImGui::Text("Componentを追加:");
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
				if (ImGui::Button("Componentを追加"))
				{
					AddComponentByName(selected_, availableComponents_[selectedCompIndex_]);
				}
			}
			ImGui::Spacing();
		}
	}
	else
	{
		ImGui::TextDisabled("一覧からGameObjectを選ぶと編集できます。");
	}

	ImGui::EndChild(); // InspectorDetailsArea

	// ------------------ 最下部: 既存 JSON のシーンロード ------------------
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();
	ImGui::Text("既存JSONを読み込み:");

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

		if (ImGui::Button("新しいオブジェクトとして読み込み"))
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
					newObj->AddComponent(std::move(compInstance), compName);
								}
							}
						}

						if (newObj->LoadJson(fileToLoad))
						{
							selected_ = newObj;
							// シーン上のギズモは選択の一元管理（SelectionContext）を見るので、そちらにも伝える
							SelectionContext::GetInstance()->SelectGameObject(newObj);
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
		ImGui::TextDisabled("JSONファイルがない。");
	}

	ImGui::SameLine();
	if (ImGui::Button("一覧を更新"))
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
		owner->AddComponent(std::move(comp), compTypeName);
	}
}

void GameObjectEditor::UpdateJsonFileList()
{
	jsonFiles_.clear();
	std::filesystem::path dirPath = PathManager::GetApplicationResourceRoot() / "json" / "gameobject";
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

void GameObjectEditor::UpdatePrefabFileList()
{
	prefabFiles_.clear();
	const std::filesystem::path directory = PathManager::GetApplicationResourceRoot() / "json" / "prefab";
	try
	{
		std::filesystem::create_directories(directory);
		for (const auto& entry : std::filesystem::directory_iterator(directory))
		{
			if (entry.is_regular_file() && entry.path().extension() == ".json")
			{
				prefabFiles_.push_back(entry.path().filename().string());
			}
		}
	}
	catch (...) {}
	std::sort(prefabFiles_.begin(), prefabFiles_.end());
	if (selectedPrefabIndex_ >= static_cast<int>(prefabFiles_.size()))
	{
		selectedPrefabIndex_ = 0;
	}
}
} // namespace KCE
