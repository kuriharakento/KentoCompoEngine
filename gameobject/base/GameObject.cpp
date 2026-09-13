#include "GameObject.h"
#include "base/PathManager.h"
#include "core/SchemaVersion.h"
#include "engine/gameobject/manager/GameObjectManager.h"

#include "engine/graphics/3d/Object3dCommon.h"
#include "manager/scene/LightManager.h"
#include "time/TimeManager.h"
// system
#include "base/Logger.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include "manager/editor/DebugUIManager.h"
#endif

#include <fstream>
#include <filesystem>

namespace KCE
{
GameObject::~GameObject()
{
	isDestroying_ = true;
	if (GameObjectManager::HasInstance())
	{
		GameObjectManager::GetInstance()->Unregister(this);
	}
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->Unregister(this);
	}
#endif
	DestroyAllComponents();
	isActive_ = false;
	renderable3d_.reset();
}

GameObject::GameObject(const std::string& tag)
{
	isActive_ = true;

	// タグの空文字列チェック
	assert(!tag.empty() && "ERROR: GameObject::GameObject() - Tag should not be empty. Ensure that you provide a valid tag.");
	tag_ = tag;

	// エディタ・シリアライズ用メンバ登録
	REGISTER_MEMBER(name_);
	REGISTER_MEMBER(tag_);
	REGISTER_MEMBER(transform_);
}

void GameObject::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, const Transform& initialTransform)
{
	// SetSkinnedModel用に参照を保持
	object3dCommon_ = object3dCommon;
	lightManager_ = lightManager;

	auto object3d = std::make_unique<Object3d>();
	object3d->Initialize(object3dCommon, object3dCommon->GetDefaultCamera());

	// デフォルトで立方体モデルを設定
	object3d->SetModel("cube");
	object3d->SetLightManager(lightManager);

	renderable3d_ = std::move(object3d);
	transform_ = initialTransform;
}

void GameObject::SetModel(const std::string& modelName)
{
	// Object3dの場合のみ有効
	if (auto* obj3d = GetObject3d())
	{
		obj3d->SetModel(modelName);
	}
}

void GameObject::SetSkinnedModel(const std::string& modelPath, const std::string& ext)
{
	if (!object3dCommon_)
	{
		return;
	}

	// 新しいSkinnedObject3dを生成
	auto skinned = std::make_unique<SkinnedObject3d>();
	skinned->Initialize(object3dCommon_, object3dCommon_->GetDefaultCamera());
	skinned->SetModel(modelPath, ext);
	if (lightManager_)
	{
		skinned->SetLightManager(lightManager_);
	}

	// renderable3d_を差し替え
	renderable3d_ = std::move(skinned);
}

Model* GameObject::GetModel() const
{
	if (auto* obj3d = GetObject3d())
	{
		return obj3d->GetModel();
	}
	return nullptr;
}

void GameObject::Update()
{
	// 更新中フラグを立てる（コンポーネントの追加・削除を保留するため）
	isUpdating_ = true;

	SyncComponentActivation();
	for (auto& component : components_)
	{
		if (component->activeInHierarchy_)
		{
			if (!component->started_)
			{
				component->Start();
				component->started_ = true;
			}
			if (!dynamic_cast<GameObjectComponent::Collider*>(component.get()))
			{
				component->Update();
			}
		}
	}

	// 3Dモデルの更新
	if (renderable3d_)
	{
		float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;
		Camera* camera = object3dCommon_ ? object3dCommon_->GetDefaultCamera() : nullptr;
		renderable3d_->Update(deltaTime, camera);
	}

	// ワールド行列の更新
	UpdateWorldMatrix();
	for (auto& component : components_)
	{
		if (component->activeInHierarchy_ && dynamic_cast<GameObjectComponent::Collider*>(component.get()))
		{
			component->Update();
		}
	}

	// 子オブジェクトの更新
	for (auto& [name, child] : children_)
	{
		if (child)
		{
			child->Update();
		}
	}

}

void GameObject::LateUpdate()
{
	for (auto& component : components_)
	{
		if (component->activeInHierarchy_)
		{
			component->LateUpdate();
		}
	}
	for (auto& [name, child] : children_)
	{
		if (child && child->IsActive()) child->LateUpdate();
	}
	isUpdating_ = false;
	ProcessPendingChanges();
}

void GameObject::Draw3D(CameraManager* camera)
{
	if (!isActive_ || !renderable3d_) { return; }

	// Transform情報をObject3Dに適用（親子関係を考慮）
	ApplyTransformToObject3D(camera);

	// 子の再帰描画でも半透明を不透明パスへ混ぜない。
	if (renderable3d_->GetRenderQueue() == RenderQueue::Opaque)
	{
		renderable3d_->Draw();
	}

	// 子オブジェクトの描画
	for (auto& [name, child] : children_)
	{
		if (child)
		{
			child->Draw3D(camera);
		}
	}

	// アクションコンポーネントの描画（エフェクト、UI、デバッグ表示など）
	for (auto& component : components_)
	{
		if (component->activeInHierarchy_)
		{
			if (auto* behaviour = dynamic_cast<GameObjectComponent::Behaviour*>(component.get()))
			{
				behaviour->Draw3D(camera);
			}
		}
	}
}

void GameObject::Draw2D()
{
	if (!isActive_) { return; }

	// アクションコンポーネントの2D描画
	for (auto& component : components_)
	{
		if (component->activeInHierarchy_)
		{
			if (auto* behaviour = dynamic_cast<GameObjectComponent::Behaviour*>(component.get()))
			{
				behaviour->Draw2D();
			}
		}
	}
	// 子オブジェクトの2D描画
	for (auto& [name, child] : children_)
	{
		if (child)
		{
			child->Draw2D();
		}
	}
}

void GameObject::DrawShadow(Camera* camera)
{
	if (!isActive_ || !renderable3d_) { return; }

	bool castShadow = true;
	if (auto* obj3d = GetObject3d())
	{
		castShadow = obj3d->GetCastShadow();
	}

	// 影を落とす設定の場合のみ描画を行う
	if (castShadow)
	{
		// シャドウパスはDraw3D前に実行されるため、ワールド行列を先に確定させる
		// （ECSのObject3dSystem::DrawShadowと同様の処理）
		renderable3d_->SetTranslate(transform_.translate);
		renderable3d_->SetRotate(transform_.rotate);
		renderable3d_->SetScale(transform_.scale);

		if (parent_ && parent_->GetRenderable3d())
		{
			Matrix4x4 localMatrix = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
			Matrix4x4 worldMatrix = localMatrix * parent_->GetRenderable3d()->GetWorldMatrix();
			renderable3d_->UpdateMatrixWithWorld(worldMatrix, camera);
		}
		else
		{
			renderable3d_->Update(0.0f, camera);
		}

		// renderable3dを通してシャドウマップへの深度書き込みを行う
		renderable3d_->DrawShadowOnly();
	}

	// 子オブジェクトのシャドウ描画
	for (auto& [name, child] : children_)
	{
		if (child)
		{
			child->DrawShadow(camera);
		}
	}
}

void GameObject::DrawGBuffer(CameraManager* camera)
{
	if (!isActive_ || !renderable3d_) { return; }

	// Transform情報をObject3Dに適用（親子関係を考慮）
	if (camera)
	{
		ApplyTransformToObject3D(camera);
	}

	// renderable3dを通してG-Bufferへの描画を行う
	if (renderable3d_->GetRenderQueue() == RenderQueue::Opaque)
	{
		renderable3d_->DrawGBuffer();
	}

	// 子オブジェクトのGBuffer描画
	for (auto& [name, child] : children_)
	{
		if (child)
		{
			child->DrawGBuffer(camera);
		}
	}
}

void GameObject::UpdateTransform(CameraManager* camera)
{
	ApplyTransformToObject3D(camera);
}

void GameObject::UpdateWorldMatrix()
{
	if (!renderable3d_) return;

	renderable3d_->SetTranslate(transform_.translate);
	renderable3d_->SetRotate(transform_.rotate);
	renderable3d_->SetScale(transform_.scale);

	// 親がいる場合は親のワールド行列と合成
	if (parent_)
	{
		Matrix4x4 localMatrix = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);

		if (parent_->GetRenderable3d())
		{
			Matrix4x4 parentWorld = parent_->GetRenderable3d()->GetWorldMatrix();
			Matrix4x4 worldMatrix = localMatrix * parentWorld;

			// 計算済みのワールド行列をrenderable3dに適用（WVPは更新しない）
			renderable3d_->UpdateMatrixWithWorld(worldMatrix, nullptr);
		}
		else
		{
			renderable3d_->UpdateWorldMatrix();
		}
	}
	else
	{
		// 親がいない場合はrenderable3d側でWorldのみ更新
		renderable3d_->UpdateWorldMatrix();
	}

	// 子オブジェクトにも変更を伝播（即時更新で当たり判定等に対応）
	for (auto& [name, child] : children_)
	{
		if (child)
		{
			child->UpdateWorldMatrix();
		}
	}
}

GameObjectComponent::Component* GameObject::AddComponent(std::unique_ptr<GameObjectComponent::Component> comp, const std::string& typeName)
{
	if (!comp)
	{
		return nullptr;
	}
	GameObjectComponent::Component* result = comp.get();
	if (isUpdating_)
	{
		pendingAdds_.emplace_back(typeName, std::move(comp));
	}
	else
	{
		AddComponentImmediate(std::move(comp), typeName);
	}
	return result;
}

void GameObject::RemoveComponent(const std::string& name)
{
	// Update実行中は保留リストに追加
	if (isUpdating_)
	{
		// 重複して積まないようにチェック
		if (std::find(pendingRemoves_.begin(), pendingRemoves_.end(), name) == pendingRemoves_.end())
		{
			pendingRemoves_.push_back(name);
		}
		return;
	}

	RemoveComponentImmediate(name);
}

void GameObject::AddChild(const std::string& name, std::unique_ptr<GameObject> child)
{
	// 同名の子オブジェクト存在チェック
	if (auto it = children_.find(name); it != children_.end())
	{
		KCE::Logger::Log("Warning: Child with name '" + name + "' already exists.");
		return;
	}

	if (child)
	{
		child->SetParent(this);
		children_[name] = std::move(child);
	}
	else
	{
		KCE::Logger::Log("Error: Attempted to add a null child GameObject.");
	}
}

GameObject* GameObject::GetChild(const std::string& name) const
{
	auto it = children_.find(name);
	if (it != children_.end())
	{
		return it->second.get();
	}
	else
	{
		KCE::Logger::Log("Warning: Child with name '" + name + "' not found.");
		return nullptr;
	}
}

void GameObject::ApplyTransformToObject3D(CameraManager* camera)
{
	if (!renderable3d_) { return; }

	renderable3d_->SetTranslate(transform_.translate);
	renderable3d_->SetRotate(transform_.rotate);
	renderable3d_->SetScale(transform_.scale);

	// 親子関係の処理
	if (parent_)
	{
		// 親がある場合：親のワールド行列と合成
		Matrix4x4 localMatrix = MakeAffineMatrix(
			transform_.scale,
			transform_.rotate,
			transform_.translate
		);

		Matrix4x4 parentWorldMatrix = parent_->renderable3d_->GetWorldMatrix();
		Matrix4x4 worldMatrix = localMatrix * parentWorldMatrix;

		renderable3d_->UpdateMatrixWithWorld(worldMatrix, camera->GetActiveCamera());
	}
	else
	{
		// 親がない場合：通常の更新処理
		renderable3d_->Update(0.0f, camera->GetActiveCamera());
	}
}

void GameObject::ShowImGuiHierarchy()
{
#ifdef USE_IMGUI
	// ユニークIDを設定（同名オブジェクトの区別のため）
	ImGui::PushID(this);

	if (ImGui::TreeNode(tag_.c_str()))
	{
		ImGui::Text("Position");
		ImGui::DragFloat3("Position", &transform_.translate.x, 0.1f);

		ImGui::Text("Rotation");
		ImGui::DragFloat3("Rotation", &transform_.rotate.x, 0.1f);

		ImGui::Text("Scale");
		ImGui::DragFloat3("Scale", &transform_.scale.x, 0.1f);

		// 子オブジェクトを再帰的に表示
		for (const auto& [name, child] : children_)
		{
			if (child)
			{
				child->ShowImGuiHierarchy();
			}
		}
		ImGui::TreePop();
	}

	ImGui::PopID();
#endif
}

GameObjectComponent::Component* GameObject::AddComponentImmediate(std::unique_ptr<GameObjectComponent::Component> comp, const std::string& typeName)
{
	if (!comp)
	{
		return nullptr;
	}
	GameObjectComponent::Component* result = comp.get();
	result->owner_ = this;
	result->Awake();
	result->awakeCalled_ = true;
	components_.push_back(std::move(comp));
	componentTypeNames_.push_back(typeName);
	SyncComponentActivation();
	return result;
}

void GameObject::RemoveComponentImmediate(const std::string& name)
{
	auto nameIt = std::find(componentTypeNames_.begin(), componentTypeNames_.end(), name);
	if (nameIt == componentTypeNames_.end())
	{
		KCE::Logger::Log("Warning: Component not found: " + name);
		return;
	}

	const size_t index = static_cast<size_t>(std::distance(componentTypeNames_.begin(), nameIt));
	auto& component = components_[index];
	if (component->activeInHierarchy_)
	{
		component->OnDisable();
	}
	component->OnDestroy();
	component->destroyed_ = true;
	components_.erase(components_.begin() + index);
	componentTypeNames_.erase(nameIt);
}

void GameObject::ProcessPendingChanges()
{
	// 削除処理を先に実行
	if (!pendingRemoves_.empty())
	{
		for (const auto& name : pendingRemoves_)
		{
			RemoveComponentImmediate(name);
		}
		pendingRemoves_.clear();
	}

	// 追加処理を実行
	if (!pendingAdds_.empty())
	{
		for (auto& [name, component] : pendingAdds_)
		{
			AddComponentImmediate(std::move(component), name);
		}
		pendingAdds_.clear();
	}
}

void GameObject::SetActive(bool isActive)
{
	if (isActive_ == isActive)
	{
		return;
	}
	isActive_ = isActive;
	SyncComponentActivation();
}

void GameObject::SyncComponentActivation()
{
	for (auto& component : components_)
	{
		const bool shouldBeActive = component->enabled_ && IsActive();
		if (shouldBeActive == component->activeInHierarchy_)
		{
			continue;
		}
		component->activeInHierarchy_ = shouldBeActive;
		if (shouldBeActive)
		{
			component->OnEnable();
		}
		else
		{
			component->OnDisable();
		}
	}
	for (auto& [name, child] : children_)
	{
		if (child) child->SyncComponentActivation();
	}
}

void GameObject::DestroyAllComponents()
{
	for (auto& component : components_)
	{
		if (component->activeInHierarchy_)
		{
			component->OnDisable();
		}
		if (!component->destroyed_)
		{
			component->OnDestroy();
			component->destroyed_ = true;
		}
	}
	components_.clear();
	componentTypeNames_.clear();
}

void GameObject::DispatchCollisionEnter(const GameObjectComponent::CollisionInfo& info)
{
	for (auto& component : components_)
	{
		if (component->activeInHierarchy_) component->OnCollisionEnter(info);
	}
}

void GameObject::DispatchCollisionStay(const GameObjectComponent::CollisionInfo& info)
{
	for (auto& component : components_)
	{
		if (component->activeInHierarchy_) component->OnCollisionStay(info);
	}
}

void GameObject::DispatchCollisionExit(const GameObjectComponent::CollisionInfo& info)
{
	for (auto& component : components_)
	{
		if (component->activeInHierarchy_) component->OnCollisionExit(info);
	}
}
void GameObject::DispatchObjectCollisionEnter(const GameObjectComponent::CollisionInfo& info)
{
	for (auto& component : components_) if (component->activeInHierarchy_) component->OnObjectCollisionEnter(info);
}
void GameObject::DispatchObjectCollisionStay(const GameObjectComponent::CollisionInfo& info)
{
	for (auto& component : components_) if (component->activeInHierarchy_) component->OnObjectCollisionStay(info);
}
void GameObject::DispatchObjectCollisionExit(const GameObjectComponent::CollisionInfo& info)
{
	for (auto& component : components_) if (component->activeInHierarchy_) component->OnObjectCollisionExit(info);
}

bool GameObject::SaveJson(const std::string& path) const
{
	std::string targetPath = path;
	if (targetPath.empty())
	{
		targetPath = name_ + ".json";
	}

	std::filesystem::path dirPath = PathManager::GetApplicationResourceRoot() / "json" / "gameobject";
	std::filesystem::create_directories(dirPath);
	std::filesystem::path fullPath = dirPath / targetPath;

	// 1. GameObject 自身のパラメータをシリアライズ
	nlohmann::json json = JsonEditableBase::Serialize();

	// 2. 各コンポーネントのJSONをシリアライズして追加
	nlohmann::json compJson = nlohmann::json::object();
	for (size_t index = 0; index < components_.size(); ++index)
	{
		const auto& compName = componentTypeNames_[index];
		const auto& comp = components_[index];
		auto* editableComp = dynamic_cast<JsonEditableBase*>(comp.get());
		if (editableComp)
		{
			compJson[compName] = editableComp->Serialize();
		}
	}

	json["components"] = compJson;

	// 3. スキーマバージョンと安定IDを付与する
	//    バージョンはフォーマット変更時に既存データを読み分けるために必須。
	json["version"] = kGameObjectSchemaVersion;
	json["guid"] = guid_.ToString();

	// 4. ファイルに出力
	std::ofstream ofs(fullPath);
	if (!ofs)
	{
		return false;
	}
	ofs << json.dump(4);
	return true;
}

bool GameObject::LoadJson(const std::string& path)
{
	std::string targetPath = path;
	if (targetPath.empty())
	{
		targetPath = name_ + ".json";
	}

	std::filesystem::path fullPath = PathManager::GetApplicationResourceRoot() / "json" / "gameobject" / targetPath;
	if (!std::filesystem::exists(fullPath))
	{
		return false;
	}

	std::ifstream ifs(fullPath);
	if (!ifs)
	{
		return false;
	}
	nlohmann::json json;
	try
	{
		ifs >> json;
		ifs.close();
	}
	catch (...)
	{
		ifs.close();
		return false;
	}

	// 1. GameObject 自身のパラメータを復元
	JsonEditableBase::Deserialize(json);

	// 保存されていた安定IDを復元する。
	// guid を持たない旧データは、コンストラクタで採番済みのものをそのまま使う。
	if (json.contains("guid") && json["guid"].is_string())
	{
		Guid loadedGuid;
		if (Guid::TryParse(json["guid"].get<std::string>(), loadedGuid))
		{
			guid_ = loadedGuid;
		}
	}

	// 2. 各コンポーネントのパラメータを復元
	if (json.contains("components") && json["components"].is_object())
	{
		auto componentsNode = json["components"];
		for (auto it = componentsNode.begin(); it != componentsNode.end(); ++it)
		{
			std::string compName = it.key();
			auto nameIt = std::find(componentTypeNames_.begin(), componentTypeNames_.end(), compName);
			if (nameIt != componentTypeNames_.end())
			{
				const size_t index = static_cast<size_t>(std::distance(componentTypeNames_.begin(), nameIt));
				auto* editableComp = dynamic_cast<JsonEditableBase*>(components_[index].get());
				if (editableComp)
				{
					editableComp->Deserialize(it.value());
				}
			}
		}
	}

	return true;
}

void GameObject::DrawImGui()
{
#ifdef USE_IMGUI
	// 階層表示を描画
	ShowImGuiHierarchy();

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// 自身のプロパティ（TransformやName）を描画
	JsonEditableBase::DrawImGui();

	// 各コンポーネントのプロパティを描画
	for (size_t index = 0; index < components_.size(); ++index)
	{
		const auto& compName = componentTypeNames_[index];
		const auto& comp = components_[index];
		auto* editableComp = dynamic_cast<JsonEditableBase*>(comp.get());
		if (editableComp)
		{
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// コンポーネント用のヘッダーセクション
			std::string headerLabel = compName + " Component";
			if (ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PushID(comp.get());
				editableComp->DrawImGui();
				ImGui::PopID();
			}
		}
	}
#endif // USE_IMGUI
}
} // namespace KCE
