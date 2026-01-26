#include "GameObject.h"

#include "engine/graphics/3d/Object3dCommon.h"
#include "time/TimeManager.h"
// system
#include "base/Logger.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif


GameObject::~GameObject()
{
	actionComponents_.clear();
	collisionComponents_.clear();
	components_.clear();
	isActive_ = false;
	renderable3d_.reset();
}

GameObject::GameObject(std::string tag)
{
	isActive_ = true;

	// タグの空文字列チェック
	assert(!tag.empty() && "ERROR: GameObject::GameObject() - Tag should not be empty. Ensure that you provide a valid tag.");
	tag_ = tag;
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
	ShowImGuiHierarchy();

	// 更新中フラグを立てる（コンポーネントの追加・削除を保留するため）
	isUpdating_ = true;

	// アクションコンポーネントの更新
	for (auto& actionComp : actionComponents_)
	{
		if (actionComp)
		{
			actionComp->Update(this);
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

	// コリジョンコンポーネントの更新（当たり判定処理）
	for (auto& collisionComp : collisionComponents_)
	{
		if (collisionComp)
		{
			collisionComp->Update(this);
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

	isUpdating_ = false;

	// 保留中のコンポーネント変更を処理
	ProcessPendingChanges();
}

void GameObject::Draw3D(CameraManager* camera)
{
	if (!renderable3d_) { return; }

	// Transform情報をObject3Dに適用（親子関係を考慮）
	ApplyTransformToObject3D(camera);

	renderable3d_->Draw();

	// 子オブジェクトの描画
	for (auto& [name, child] : children_)
	{
		if (child)
		{
			child->Draw3D(camera);
		}
	}

	// アクションコンポーネントの描画（エフェクト、UI、デバッグ表示など）
	for (auto& actionComp : actionComponents_)
	{
		if (actionComp)
		{
			actionComp->Draw3D(camera);
		}
	}
}

void GameObject::Draw2D()
{
	// アクションコンポーネントの2D描画
	for (auto& actionComp : actionComponents_)
	{
		if (actionComp)
		{
			actionComp->Draw2D();
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

void GameObject::DrawShadow()
{
	if (!renderable3d_) { return; }

	// renderable3dを通してシャドウマップへの描画（深度のみ）を行う
	renderable3d_->DrawShadowOnly();

	// 子オブジェクトのシャドウ描画
	for (auto& [name, child] : children_)
	{
		if (child)
		{
			child->DrawShadow();
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

void GameObject::AddComponent(const std::string& name, std::unique_ptr<IGameObjectComponent> comp)
{
	// nullポインタチェック
	if (!comp)
	{
		Logger::Log("Error: Attempted to add a null component with name: " + name);
		return;
	}

	// 同名コンポーネントの重複チェック
	if (components_.find(name) != components_.end())
	{
		Logger::Log("Warning: Component already exists: " + name);
		return;
	}

	auto sharedComp = std::shared_ptr<IGameObjectComponent>(std::move(comp));

	// Update実行中は保留リストに追加
	if (isUpdating_)
	{
		pendingAdds_.emplace_back(name, sharedComp);
	}
	else
	{
		AddComponentImmediate(name, sharedComp);
	}
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

void GameObject::AddChild(const std::string name, std::unique_ptr<GameObject> child)
{
	// 同名の子オブジェクト存在チェック
	if (auto it = children_.find(name); it != children_.end())
	{
		Logger::Log("Warning: Child with name '" + name + "' already exists.");
		return;
	}

	if (child)
	{
		child->SetParent(this);
		children_[name] = std::move(child);
	}
	else
	{
		Logger::Log("Error: Attempted to add a null child GameObject.");
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
		Logger::Log("Warning: Child with name '" + name + "' not found.");
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

void GameObject::AddComponentImmediate(const std::string& name, std::shared_ptr<IGameObjectComponent> comp)
{
	if (!comp) return;

	// 重複チェック（安全のため再確認）
	if (components_.find(name) != components_.end())
	{
		Logger::Log("Warning: Component already exists on immediate add: " + name);
		return;
	}

	components_[name] = comp;

	// 型ごとにカテゴリ配列へ登録（高速アクセス用）
	if (auto action = std::dynamic_pointer_cast<IActionComponent>(comp))
	{
		actionComponents_.push_back(action);
	}
	if (auto collision = std::dynamic_pointer_cast<ICollisionComponent>(comp))
	{
		collisionComponents_.push_back(collision);
	}
}

void GameObject::RemoveComponentImmediate(const std::string& name)
{
	auto it = components_.find(name);

	if (it == components_.end())
	{
		Logger::Log("Warning: Component not found: " + name);
		return;
	}

	auto comp = it->second;

	// カテゴリ配列から削除
	RemoveFromCategoryLists(comp);

	components_.erase(it);
}

void GameObject::RemoveFromCategoryLists(const std::shared_ptr<IGameObjectComponent>& comp)
{
	if (!comp) return;

	// アクションコンポーネント配列から削除
	if (auto action = std::dynamic_pointer_cast<IActionComponent>(comp))
	{
		actionComponents_.erase(
			std::remove(actionComponents_.begin(), actionComponents_.end(), action),
			actionComponents_.end()
		);
	}

	// コリジョンコンポーネント配列から削除
	if (auto collision = std::dynamic_pointer_cast<ICollisionComponent>(comp))
	{
		collisionComponents_.erase(
			std::remove(collisionComponents_.begin(), collisionComponents_.end(), collision),
			collisionComponents_.end()
		);
	}
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
		for (auto& p : pendingAdds_)
		{
			const auto& name = p.first;
			const auto& comp = p.second;

			// 既に存在する場合は追加をスキップ
			if (components_.find(name) != components_.end())
			{
				Logger::Log("Warning: Component already exists when processing pending add: " + name + " - Skipped.");
				continue;
			}

			AddComponentImmediate(name, comp);
		}
		pendingAdds_.clear();
	}
}