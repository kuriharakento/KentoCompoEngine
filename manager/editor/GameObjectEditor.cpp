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
#include "graphics/3d/Model.h"
#include "graphics/3d/Object3d.h"
#include "manager/graphics/ModelManager.h"
#include "manager/graphics/TextureManager.h"
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
	// レンダラー（モデル・スプライト・ゲージ）は追加ではなく選んで切り替えるので、一覧から外す
	availableComponents_.clear();
	for (const std::string& name : GameObjectComponent::ComponentFactory::GetInstance()->GetRegisteredNames())
	{
		if (!GameObjectComponent::ComponentFactory::GetInstance()->IsRenderer(name))
		{
			availableComponents_.push_back(name);
		}
	}

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
		// 画面（px）に置く 2D は、3D のギズモでは動かせない（数 px しか動かない）。位置はインスペクタの数値で決める
		if (object->HasScreenSpaceSprite())
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
	// 複製も同じ理由で、描き終えてから行う
	GameObject* objectToDuplicate = nullptr;
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
		// 描画のオンオフ。更新は止まらない（止めるなら、インスペクタの「有効」）
		{
			// 一覧は根だけなので、親の影響は無い
			bool visible = obj->IsVisible();
			if (ImGui::Checkbox("##visible", &visible))
			{
				obj->SetVisible(visible);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("表示（描画だけオンオフ。更新は止まらない）");
			}
			ImGui::SameLine();
		}
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
			if (ImGui::MenuItem("複製", "Ctrl+D"))
			{
				objectToDuplicate = obj;
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

	// Ctrl+D でも複製する（一覧にフォーカスがあるとき）
	if (selected_ && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
	{
		objectToDuplicate = selected_;
	}
	if (objectToDuplicate)
	{
		// プレハブの形にして作り直す（コンポーネントの値も、子も一緒に写る）。新しい GUID が付く
		if (auto copy = GameObjectManager::GetInstance()->CreateFromPrefabJson(objectToDuplicate->SerializePrefab()))
		{
			copy->SetName(objectToDuplicate->GetName() + "_copy");
			if (GameObject* added = GameObjectManager::GetInstance()->Adopt(std::move(copy)))
			{
				selected_ = added;
				SelectionContext::GetInstance()->SelectGameObject(added);
			}
		}
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
		// 最初はモデルレンダラー。本体の立方体はレンダラーが代わりに描くので隠す
		AddComponentByName(newObj, "ModelRendererComponent");
		newObj->SetBodyVisible(false);
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
		// 本体を隠している（レンダラーが描く）ときは、本体の見た目は出さない
		if (IRenderable3d* renderable = selected_->IsBodyVisible() ? selected_->GetRenderable3d() : nullptr)
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

				// テクスチャ（メッシュごと）。読み込み済みの画像から選ぶ。キューブマップなど、2D でない物は出さない
				Model* model = selected_->GetObject3d() ? selected_->GetObject3d()->GetModel() : nullptr;
				if (model && model->GetMeshCount() > 0)
				{
					const auto textures = TextureManager::GetInstance()->GetLoadedTextures();
					const auto& meshTextures = selected_->GetMeshTextures();
					for (size_t meshIndex = 0; meshIndex < model->GetMeshCount(); ++meshIndex)
					{
						const std::string current = meshIndex < meshTextures.size() ? meshTextures[meshIndex] : std::string();
						const std::string label = model->GetMeshCount() == 1 ? std::string("テクスチャ") : "テクスチャ " + std::to_string(meshIndex + 1);
						ImGui::PushID(static_cast<int>(meshIndex));
						if (ImGui::BeginCombo(label.c_str(), current.empty() ? "（モデルのまま）" : current.c_str()))
						{
							if (ImGui::Selectable("（モデルのまま）", current.empty()))
							{
								selected_->SetMeshTexture(meshIndex, std::string());
							}
							for (const auto& texture : textures)
							{
								if (!texture.previewable) { continue; }
								if (ImGui::Selectable(texture.path.c_str(), texture.path == current))
								{
									selected_->SetMeshTexture(meshIndex, texture.path);
								}
								// 選ぶ前に中身が分かるよう、重ねた項目にカーソルを置くとプレビューを出す
								if (ImGui::IsItemHovered() && texture.gpuHandle != 0 && texture.width > 0 && texture.height > 0)
								{
									const float scale = (std::min)(192.0f / static_cast<float>(texture.width), 192.0f / static_cast<float>(texture.height));
									ImGui::BeginTooltip();
									ImGui::Image(static_cast<ImTextureID>(texture.gpuHandle), ImVec2(static_cast<float>(texture.width) * scale, static_cast<float>(texture.height) * scale));
									ImGui::TextDisabled("%u x %u", texture.width, texture.height);
									ImGui::EndTooltip();
								}
							}
							ImGui::EndCombo();
						}
						// いま使っているテクスチャの小さなプレビュー
						if (!current.empty())
						{
							const auto now = std::find_if(textures.begin(), textures.end(), [&current](const auto& tex) { return tex.path == current; });
							if (now != textures.end() && now->previewable && now->gpuHandle != 0 && now->width > 0 && now->height > 0)
							{
								const float scale = (std::min)(64.0f / static_cast<float>(now->width), 64.0f / static_cast<float>(now->height));
								ImGui::Image(static_cast<ImTextureID>(now->gpuHandle), ImVec2(static_cast<float>(now->width) * scale, static_cast<float>(now->height) * scale));
							}
						}
						ImGui::PopID();
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("読み込み済みのテクスチャから選ぶ。Project ウィンドウで、読み込める。\n替えた物は、同じモデルの物とまとめて描かれず、1体ずつ描かれる");
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

		// レンダラー。追加ではなく選んで切り替える（モデル・スプライト・ゲージなど、描画を担うコンポーネント）
		{
			auto* factory = GameObjectComponent::ComponentFactory::GetInstance();
			const auto& rendererNames = factory->GetRendererNames();
			const auto& compNames = selected_->GetComponentTypeNames();
			std::string currentRenderer;
			for (const std::string& name : compNames)
			{
				if (factory->IsRenderer(name)) { currentRenderer = name; break; }
			}
			if (!rendererNames.empty())
			{
				InspectorSection("レンダラー");
				// レンダラーが無い物（古いデータなど）は、本体のモデルで描かれている
				if (ImGui::BeginCombo("種類", currentRenderer.empty() ? "（本体のモデル）" : currentRenderer.c_str()))
				{
					std::string nextRenderer = currentRenderer;
					bool changed = false;
					for (const std::string& name : rendererNames)
					{
						if (ImGui::Selectable(name.c_str(), name == currentRenderer))
						{
							nextRenderer = name;
							changed = true;
						}
					}
					ImGui::EndCombo();
					if (changed && nextRenderer != currentRenderer)
					{
						// いまのレンダラーを外して、選んだものに差し替える
						if (!currentRenderer.empty()) { selected_->RemoveComponent(currentRenderer); }
						AddComponentByName(selected_, nextRenderer);
						// 選んだレンダラーが描くので、本体の立方体は隠す
						selected_->SetBodyVisible(false);
					}
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("描画を担うもの。1つだけ選べる。\n新しく作った物は、モデルから始まる");
				}
				ImGui::Spacing();
			}
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
				// レンダラーは上の「レンダラー」で切り替えるので、ここには削除を出さない
				if (!GameObjectComponent::ComponentFactory::GetInstance()->IsRenderer(compName))
				{
					ImGui::SameLine(ImGui::GetContentRegionMax().x - deleteWidth);
					if (ImGui::SmallButton("削除"))
					{
						removeName = compName;
					}
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
