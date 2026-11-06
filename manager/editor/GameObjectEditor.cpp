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
#include "graphics/3d/IRenderable3d.h"
#include "manager/graphics/ModelManager.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

// Factory
#include "engine/gameobject/component/base/ComponentFactory.h"

namespace KCE
{
namespace
{
/**
 * @brief Inspector の見出し。折りたたまず、線つきの文字で区切る。中身は常に表示する
 */
void InspectorSection(const char* label)
{
	ImGui::Spacing();
	ImGui::SeparatorText(label);
}
} // namespace

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
	// シーン上のクリックなど、ここ以外で選ばれても行の強調がずれないよう、選択の一元管理に合わせる
	selected_ = SelectionContext::GetInstance()->GetPrimaryGameObject();

	if (ImGui::Button("GameObjectを作成", ImVec2(-FLT_MIN, 0.0f)))
	{
		CreateAndSelectNew();
	}

	// 行の上で右クリックしたときの削除は、描画中に一覧を変えないよう、描き終えてから行う
	GameObject* objectToDelete = nullptr;
	const auto& objects = GameObjectManager::GetInstance()->GetGameObjects();
	for (GameObject* obj : objects)
	{
		if (!obj) continue;

		const std::string label = obj->GetName() + " (" + obj->GetTag() + ")";
		if (!DebugUIManager::GetInstance()->MatchesHierarchyFilter(label))
		{
			continue;
		}

		// 同じ名前と種類の GameObject があっても、行の ID がぶつからないようにする
		ImGui::PushID(obj);
		if (ImGui::Selectable(label.c_str(), selected_ == obj))
		{
			selected_ = obj;
			// シーン上のギズモは選択の一元管理（SelectionContext）を見るので、そちらにも伝える
			SelectionContext::GetInstance()->SelectGameObject(obj);
		}
		if (ImGui::BeginPopupContextItem("GameObjectRowMenu"))
		{
			// 右クリックした行を選んでから出す。削除の対象がどれか分かるようにする
			if (selected_ != obj)
			{
				selected_ = obj;
				SelectionContext::GetInstance()->SelectGameObject(obj);
			}
			if (ImGui::MenuItem("削除"))
			{
				objectToDelete = obj;
			}
			ImGui::EndPopup();
		}
		ImGui::PopID();
	}

	// 一覧の空いている所で右クリックしても作れる
	if (ImGui::BeginPopupContextWindow("GameObjectListMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::MenuItem("GameObjectを作成"))
		{
			CreateAndSelectNew();
		}
		ImGui::EndPopup();
	}

	if (objectToDelete)
	{
		// 不正ポインタ参照を防ぐため、Unregister前にselected_をクリア
		selected_ = nullptr;
		GameObjectManager::GetInstance()->Unregister(objectToDelete);
	}

#endif
}

void GameObjectEditor::CreateAndSelectNew()
{
#ifdef USE_IMGUI
	GameObject* newObj = GameObjectManager::GetInstance()->CreateGameObject("NewObject", "GameObject");
	if (newObj)
	{
		// 初期の Deferred は G-Buffer を描かないシーンだと映らないので、どのシーンでも映る Forward にしておく
		if (IRenderable3d* renderable = newObj->GetRenderable3d())
		{
			renderable->SetRenderingType(RenderingType::Forward);
		}
		selected_ = newObj;
		// シーン上のギズモは選択の一元管理（SelectionContext）を見るので、そちらにも伝える
		SelectionContext::GetInstance()->SelectGameObject(newObj);
	}
#endif
}

void GameObjectEditor::DrawInspectorImGui()
{
#ifdef USE_IMGUI

	// 子のスクロール領域は作らない（ウィンドウのスクロールと二重になり、ホイールが使いにくいため）

	if (selected_)
	{
		// 名前・Transform（一番よく触るので最上部）
		selected_->DrawImGui();
		ImGui::Spacing();

		// 見た目（色・描画方式）。本体の描画物がある物だけ出す
		if (IRenderable3d* renderable = selected_->GetRenderable3d())
		{
			InspectorSection("見た目");
			{
				// モデル（静的モデルの物だけ）。読み込み済みのモデルから選ぶ
				if (selected_->GetObject3d())
				{
					const std::string currentModel = selected_->GetModelName();
					if (ImGui::BeginCombo("モデル", currentModel.empty() ? "（なし）" : currentModel.c_str()))
					{
						for (const std::string& modelName : ModelManager::GetInstance()->GetLoadedModelNames())
						{
							if (ImGui::Selectable(modelName.c_str(), modelName == currentModel))
							{
								// 古いモデルの GPU リソースを描画の途中で壊さないよう、次のフレームの頭に差し替える
								GameObjectManager::GetInstance()->RequestModelChange(selected_, modelName);
							}
						}
						ImGui::EndCombo();
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("読み込み済みのモデルから選ぶ。次のフレームで変わる。変えると素材の設定（色など）は読み込んだ状態に戻るので、色はあとで直す");
					}
				}

				Vector4 color = renderable->GetColor();
				if (ImGui::ColorEdit4("色", &color.x))
				{
					renderable->SetColor(color);
				}

				int renderingType = static_cast<int>(renderable->GetRenderingType());
				const char* const kRenderingTypeLabels[] = { "Deferred", "Forward" };
				if (ImGui::Combo("描画方式", &renderingType, kRenderingTypeLabels, IM_ARRAYSIZE(kRenderingTypeLabels)))
				{
					renderable->SetRenderingType(static_cast<RenderingType>(renderingType));
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("映らないときは Forward にする。Deferred は G-Buffer を描くシーンでだけ映る");
				}
			}
			ImGui::Spacing();
		}

		// コンポーネント。見出しごとに折りたたみ、右端の「削除」で外す
		{
			std::string removeName;
			const auto& comps = selected_->GetComponents();
			const auto& componentNames = selected_->GetComponentTypeNames();
			for (size_t index = 0; index < comps.size(); ++index)
			{
				const std::string& compName = componentNames[index];
				// 同じ種類を2つ持てるので、種類名ではなく並び順で ID を分ける
				ImGui::PushID(static_cast<int>(index));
				const float deleteWidth = ImGui::CalcTextSize("削除").x + ImGui::GetStyle().FramePadding.x * 2.0f;
				InspectorSection(compName.c_str());
				ImGui::SameLine(ImGui::GetContentRegionMax().x - deleteWidth);
				if (ImGui::SmallButton("削除"))
				{
					removeName = compName;
				}
				{
					if (auto* editable = dynamic_cast<JsonEditableBase*>(comps[index].get()))
					{
						editable->DrawImGui();
					}
					else
					{
						ImGui::TextDisabled("編集できる項目はありません。");
					}
				}
				ImGui::PopID();
			}
			if (!removeName.empty())
			{
				selected_->RemoveComponent(removeName);
			}
		}

		// コンポーネントの追加
		if (!availableComponents_.empty())
		{
			ImGui::Spacing();
			ImGui::SetNextItemWidth(-ImGui::CalcTextSize("追加").x - ImGui::GetStyle().FramePadding.x * 2.0f - ImGui::GetStyle().ItemSpacing.x);
			if (ImGui::BeginCombo("##CompSelect", availableComponents_[selectedCompIndex_].c_str()))
			{
				for (int i = 0; i < static_cast<int>(availableComponents_.size()); ++i)
				{
					if (ImGui::Selectable(availableComponents_[i].c_str(), selectedCompIndex_ == i))
					{
						selectedCompIndex_ = i;
					}
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			if (ImGui::Button("追加"))
			{
				AddComponentByName(selected_, availableComponents_[selectedCompIndex_]);
			}
		}

		// 保存（使う頻度が低いので、最後に折りたたんで置く）
		ImGui::Spacing();
		// 1. JSON ファイル操作（折りたたみ可能にしてスッキリさせる）
		InspectorSection("JSON保存");
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

		InspectorSection("プレハブ");
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

	}
	else
	{
		ImGui::TextDisabled("一覧からGameObjectを選ぶと編集できます。");
	}

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
