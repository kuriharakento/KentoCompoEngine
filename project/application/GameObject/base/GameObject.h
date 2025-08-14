#pragma once
#include <memory>
#include <string>

// graphics
#include "graphics/3d/Object3d.h"
// math
#include "base/GraphicsTypes.h"
// component
#include "application/GameObject/component/base/IGameObjectComponent.h"

class GameObject
{
public:
	virtual ~GameObject();
	explicit GameObject(std::string tag = "");	// コンストラクタ
	virtual void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, Camera* camera = nullptr);		// 初期化
	virtual void Update();
	virtual void Draw(CameraManager* camera);
	void UpdateTransform(CameraManager* camera);	// Transform情報の更新
	void AddComponent(const std::string& name, std::unique_ptr<IGameObjectComponent> comp);	// コンポーネントの追加
	template<typename T>
	std::shared_ptr<T> GetComponent() const;
public: //アクセッサ
	//トランスフォーム
	virtual void SetPosition(const Vector3& pos) { transform_.translate = pos; }
	virtual void SetRotation(const Vector3& rot) { transform_.rotate = rot; }
	virtual void SetScale(const Vector3& scale) { transform_.scale = scale; }
	virtual const Vector3& GetPosition() const { return transform_.translate; }
	virtual const Vector3& GetRotation() const { return transform_.rotate; }
	virtual const Vector3& GetScale() const { return transform_.scale; }

	//オブジェクト3D
	void SetModel(const std::string& modelName) { object3d_->SetModel(modelName); }	// モデルの設定
	Model* GetModel() const { return object3d_ ? object3d_->GetModel() : nullptr; }	// モデルの取得

	//タグ
	std::string GetTag() const { return tag_; }	// タグの取得
	void SetTag(const std::string& tag) { tag_ = tag; }	// タグの設定
	//アクティブ状態
	bool IsActive() const { return isActive_; }	// アクティブ状態の取得

	// 親子関係
	void SetParent(GameObject* parent) { parent_ = parent; }	// 親オブジェクトの設定
	void AddChild(std::unique_ptr<GameObject> child);	// 子オブジェクトの追加

protected:
	Transform transform_;																	// Transform情報
	std::unique_ptr<Object3d> object3d_;													// 3Dオブジェクト

private:
	void ApplyTransformToObject3D(CameraManager* camera);											// Transform情報をObject3Dに適用

private:
	std::unordered_map<std::string, std::shared_ptr<IGameObjectComponent>> components_;		// コンポーネントのリスト
	std::string tag_; 																		// オブジェクトのタグ
	bool isActive_;																			// アクティブ状態
	std::vector<std::unique_ptr<GameObject>> children_;  // 子オブジェクトのリスト
	GameObject* parent_ = nullptr;  // 親オブジェクト
};

template <typename T>
std::shared_ptr<T> GameObject::GetComponent() const
{
	for (const auto& [_, comp] : components_)
	{
		if (auto casted = std::dynamic_pointer_cast<T>(comp))
		{
			return casted;
		}
	}
	return nullptr;
}