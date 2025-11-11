#include "GameObject.h"

#include "engine/graphics/3d/Object3dCommon.h"
// system
#include "base/Logger.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif


GameObject::~GameObject()
{
	actionComponents_.clear();				// アクションコンポーネントのクリア
	collisionComponents_.clear();			// コリジョンコンポーネントのクリア
	components_.clear();					// 全コンポーネントのクリア
	isActive_ = false;						// 非アクティブ状態に設定
	object3d_.reset();						// Object3Dのリセット（スマートポインタの自動削除）
}

GameObject::GameObject(std::string tag)
{
	// アクティブ状態で初期化
	isActive_ = true;

	// タグの初期化（空文字列チェック）
	assert(!tag.empty() && "ERROR: GameObject::GameObject() - Tag should not be empty. Ensure that you provide a valid tag.");
	tag_ = tag;
}

void GameObject::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, const Transform& initialTransform)
{
	// 3Dオブジェクトの初期化
	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(object3dCommon, object3dCommon->GetDefaultCamera());

	// デフォルトで立方体モデルを設定
	object3d_->SetModel("cube");
	object3d_->SetLightManager(lightManager);

}


void GameObject::Update()
{
	ShowImGuiHierarchy(); // ImGuiでの階層表示（デバッグ用）

	// 更新中フラグを立てる（コンポーネントの追加・削除を保留するため）
	isUpdating_ = true;

	// アクションコンポーネントの更新（移動、アニメーションなど）
	for (auto& actionComp : actionComponents_)
	{
		if (actionComp)
		{
			actionComp->Update(this); // アクションコンポーネントの更新
		}
	}

	// ワールド行列の更新（親子関係を考慮した座標変換）
	UpdateWorldMatrix();

	// コリジョンコンポーネントの更新（当たり判定処理）
	for (auto& collisionComp : collisionComponents_)
	{
		if (collisionComp)
		{
			collisionComp->Update(this); // コリジョンコンポーネントの更新
		}
	}

	// 子オブジェクトの再帰的更新
	for (auto& [name, child] : children_)
	{
		if (child)
		{
			child->Update(); // 子オブジェクトの更新
		}
	}

	// 更新終了フラグを下ろす
	isUpdating_ = false;

	// 保留中のコンポーネント変更を処理
	ProcessPendingChanges();
}

void GameObject::Draw(CameraManager* camera)
{
	// Object3Dが存在しない場合は早期リターン
	if (!object3d_) { return; }

	// Transform情報をObject3Dに適用（親子関係を考慮）
	ApplyTransformToObject3D(camera);

	// 3Dオブジェクトの描画
	object3d_->Draw();

	// 子オブジェクトの描画（階層順に描画）
	for (auto& [name, child] : children_)
	{
		if (child)
		{
			child->Draw(camera); // 子オブジェクトの描画
		}
	}

	// アクションコンポーネントの描画処理
	// （エフェクト、UI、デバッグ表示などを含む）
	for (auto& actionComp : actionComponents_)
	{
		if (actionComp)
		{
			actionComp->Draw(camera); // アクションコンポーネントの描画
		}
	}
}

void GameObject::UpdateTransform(CameraManager* camera)
{
	// Transform情報をObject3Dに適用
	ApplyTransformToObject3D(camera);
}

void GameObject::UpdateWorldMatrix()
{
	// Object3Dがなければ処理しない
	if (!object3d_) return;

	// Object3Dにローカルトランスフォームを設定
	object3d_->SetTranslate(transform_.translate);
	object3d_->SetRotate(transform_.rotate);
	object3d_->SetScale(transform_.scale);

	// 親がいる場合は親のワールド行列と合成
	if (parent_)
	{
		// 自身のローカル行列を作成
		Matrix4x4 localMatrix = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);

		// 親のワールド行列を取得
		if (parent_->GetObject3d())
		{
			Matrix4x4 parentWorld = parent_->GetObject3d()->GetWorldMatrix();
			Matrix4x4 worldMatrix = localMatrix * parentWorld;	// 行列の合成

			// 計算済みのワールド行列をObject3Dに適用（WVPは更新しない）
			object3d_->UpdateMatrixWithWorld(worldMatrix, nullptr);
		}
		else
		{
			// 親にObject3Dが無ければ通常のワールド更新
			object3d_->UpdateWorldMatrix();
		}
	}
	else
	{
		// 親がいない場合はObject3D側でWorldのみ更新
		object3d_->UpdateWorldMatrix();
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

	// unique_ptrからshared_ptrに変換
	auto sharedComp = std::shared_ptr<IGameObjectComponent>(std::move(comp));

	// Update実行中は保留リストに追加
	if (isUpdating_)
	{
		pendingAdds_.emplace_back(name, sharedComp);
	}
	else
	{
		// 通常時は即座に追加
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

	// 通常時は即座に削除
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

	// 有効な子オブジェクトの場合
	if (child)
	{
		child->SetParent(this); // 親オブジェクトを設定
		children_[name] = std::move(child); // 子オブジェクトを追加
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
		return it->second.get(); // 子オブジェクトを返す
	}
	else
	{
		Logger::Log("Warning: Child with name '" + name + "' not found.");
		return nullptr; // 見つからなかった場合はnullptrを返す
	}
}

void GameObject::ApplyTransformToObject3D(CameraManager* camera)
{
	// Object3Dが存在しない場合は早期リターン
	if (!object3d_) { return; }

	// ローカルトランスフォームをObject3Dに設定
	object3d_->SetTranslate(transform_.translate);
	object3d_->SetRotate(transform_.rotate);
	object3d_->SetScale(transform_.scale);

	// 親子関係の処理
	if (parent_)
	{
		// 親がある場合：親のワールド行列と合成

		// 自分のローカル行列を作成
		Matrix4x4 localMatrix = MakeAffineMatrix(
			transform_.scale,
			transform_.rotate,
			transform_.translate
		);

		// 親のワールド行列を取得
		Matrix4x4 parentWorldMatrix = parent_->object3d_->GetWorldMatrix();

		// 正しい順序で行列を合成: localMatrix * parentWorldMatrix
		Matrix4x4 worldMatrix = localMatrix * parentWorldMatrix;

		// Object3Dに計算済みのワールド行列を設定
		object3d_->UpdateMatrixWithWorld(worldMatrix, camera->GetActiveCamera());
	}
	else
	{
		// 親がない場合：通常の更新処理
		object3d_->Update(camera);
	}
}

void GameObject::ShowImGuiHierarchy()
{
#ifdef USE_IMGUI
	// ユニークIDを設定（同名オブジェクトの区別のため）
	ImGui::PushID(this);

	// ツリーノードで親子関係を表示
	if (ImGui::TreeNode(tag_.c_str()))
	{
		// Position（位置）の編集
		ImGui::Text("Position");
		ImGui::DragFloat3("Position", &transform_.translate.x, 0.1f);

		// Rotation（回転）の編集
		ImGui::Text("Rotation");
		ImGui::DragFloat3("Rotation", &transform_.rotate.x, 0.1f);

		// Scale（スケール）の編集
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
	// nullポインタチェック
	if (!comp) return;

	// 重複チェック（安全のため再確認）
	if (components_.find(name) != components_.end())
	{
		Logger::Log("Warning: Component already exists on immediate add: " + name);
		return;
	}

	// コンポーネントマップに追加
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

	// 指定された名前のコンポーネントが存在しない場合は早期リターン
	if (it == components_.end())
	{
		Logger::Log("Warning: Component not found: " + name);
		return;
	}

	auto comp = it->second;

	// カテゴリ配列から削除
	RemoveFromCategoryLists(comp);

	// コンポーネントマップから完全に削除
	components_.erase(it);
}

void GameObject::RemoveFromCategoryLists(const std::shared_ptr<IGameObjectComponent>& comp)
{
	// nullポインタチェック
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
			// 即時削除処理を実行（この時点では更新中ではない）
			RemoveComponentImmediate(name);
		}
		pendingRemoves_.clear(); // 保留リストをクリア
	}

	// 追加処理を実行
	if (!pendingAdds_.empty())
	{
		for (auto& p : pendingAdds_)
		{
			const auto& name = p.first;
			const auto& comp = p.second;

			// 既に存在する場合は追加をスキップ
			// （必要に応じてReplaceComponent機能を実装）
			if (components_.find(name) != components_.end())
			{
				Logger::Log("Warning: Component already exists when processing pending add: " + name + " - Skipped.");
				continue;
			}

			AddComponentImmediate(name, comp);
		}
		pendingAdds_.clear(); // 保留リストをクリア
	}
}