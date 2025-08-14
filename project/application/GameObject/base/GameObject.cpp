#include "GameObject.h"

// component
#include "application/GameObject/component/base/IActionComponent.h"
// system
#include "base/Logger.h"
#include "imgui/imgui.h"

GameObject::~GameObject()
{
	components_.clear(); // コンポーネントのクリア
	isActive_ = false;    // 非アクティブ状態に設定
	object3d_.reset(); // Object3Dのリセット
}

GameObject::GameObject(std::string tag)
{
	// アクティブ状態
	isActive_ = true;
	// タグの初期化
	assert(!tag.empty() && "ERROR: GameObject::GameObject() - Tag should not be empty. Ensure that you provide a valid tag.");
	tag_ = tag;
}

void GameObject::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, Camera* camera)
{
	// 3Dオブジェクトの初期化
	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(object3dCommon, camera);
	// デフォルトで立方体モデルを設定
	object3d_->SetModel("cube");
	object3d_->SetLightManager(lightManager);
	// Transformの初期化
	transform_ = {
		{ 1.0f, 1.0f, 1.0f }, // scale
		{ 0.0f, 0.0f, 0.0f }, // rotate
		{ 0.0f, 0.0f, 0.0f }  // translate
	};
	// コンポーネントの初期化
	components_.clear();
	// GameObjectManagerに登録
}

void GameObject::Update()
{
	// コンポーネントを更新
	for (auto& [name, comp] : components_)
	{
		comp->Update(this); // コンポーネントの更新
	}

	// 子オブジェクトもコンポーネントを更新
	for (const auto& child : children_)
	{
		if (child)
		{
			child->Update(); // 子オブジェクトの更新
		}
	}
}

void GameObject::Draw(CameraManager* camera)
{
	if (!object3d_) { return; }

	// Transform情報をObject3Dに適用
	ApplyTransformToObject3D(camera);

	// 3Dオブジェクトの描画
	object3d_->Draw();

	// 子オブジェクトの描画
	for (const auto& child : children_)
	{
		if (child)
		{
			child->Draw(camera); // 子オブジェクトの描画
		}
	}

	// コンポーネントを更新
	for (auto& [name, comp] : components_)
	{
		// IActionComponent にキャスト可能か確認
		if (auto actionComp = std::dynamic_pointer_cast<IActionComponent>(comp))
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

void GameObject::AddComponent(const std::string& name, std::unique_ptr<IGameObjectComponent> comp)
{
	//すでに同じ名前のコンポーネントが存在する場合はメッセージを出力
	if (components_.find(name) != components_.end())
	{
		Logger::Log("Warning: Component already exists: " + name);
	}
	// コンポーネントを追加
	components_[name] = std::move(comp);
}

void GameObject::AddChild(std::unique_ptr<GameObject> child)
{
	if (child)
	{
		child->SetParent(this); // 親を設定
		// 子オブジェクトを追加
		children_.push_back(std::move(child));
	}
}

void GameObject::ApplyTransformToObject3D(CameraManager* camera)
{
	if (!object3d_) { return; }

	object3d_->SetTranslate(transform_.translate);
	object3d_->SetRotate(transform_.rotate);
	object3d_->SetScale(transform_.scale);

	if (parent_)
	{
		// 親がある場合の処理
		// 自分のローカル行列を作成
		Matrix4x4 localMatrix = MakeAffineMatrix(
			transform_.scale,
			transform_.rotate,
			transform_.translate
		);

		// 親のワールド行列を取得
		Matrix4x4 parentWorldMatrix = parent_->object3d_->GetWorldMatrix();

		// 正しい順序で行列を掛ける: localMatrix * parentWorldMatrix
		Matrix4x4 worldMatrix = localMatrix * parentWorldMatrix;

		// Object3Dに計算済みのワールド行列を設定
		object3d_->UpdateMatrixWithWorld(worldMatrix, camera->GetActiveCamera());
	}
	else
	{
		// 親がない場合は通常の更新
		object3d_->Update(camera);
	}
}
