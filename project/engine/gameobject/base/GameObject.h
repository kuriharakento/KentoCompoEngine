#pragma once
#include <memory>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <vector>

// graphics
#include "graphics/3d/IRenderable3d.h"
#include "graphics/3d/Object3d.h"
#include "graphics/3d/SkinnedObject3d.h"
// math
#include "base/GraphicsTypes.h"
// component
#include "engine/gameobject/component/base/IActionComponent.h"
#include "engine/gameobject/component/base/ICollisionComponent.h"

namespace GameObjectComponent
{
	class IGameObjectComponent;
	class IActionComponent;
	class ICollisionComponent;
}

/**
 * @brief ゲーム内の全てのオブジェクトの基底クラス
 * 
 * Entity-Component-System(ECS)パターンを実装し、
 * 3D空間でのトランスフォーム、描画、親子関係を管理します。
 * コンポーネントを動的に追加・削除することで機能を拡張できます。
 * 
 * 主な機能:
 * - トランスフォーム管理（位置、回転、スケール）
 * - コンポーネントシステム（機能の動的追加・削除）
 * - 親子関係による階層構造
 * - 3D描画
 * 
 * @note コンポーネントの追加・削除は更新中に保留され、更新終了後に処理されます
 */
class GameObject
{
public:
	/**
	 * @brief デストラクタ
	 * 
	 * 全てのコンポーネントをクリアし、オブジェクトを非アクティブ状態にします。
	 */
	virtual ~GameObject();

	/**
	 * @brief コンストラクタ
	 * @param tag オブジェクトのタグ（デフォルト: GameObjectTag::Common::GameObject）
	 */
	explicit GameObject(const std::string& tag = "GameObject");

	/**
	 * @brief GameObjectの初期化
	 * @param object3dCommon 3Dオブジェクト共通データ
	 * @param lightManager ライト管理クラス
	 * @param initialTransform 初期トランスフォーム
	 */
	virtual void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, const Transform& initialTransform = Transform());

	/**
	 * @brief 毎フレームの更新処理
	 * 
	 * コンポーネント、ワールド行列、子オブジェクトを更新します。
	 * 更新中のコンポーネント追加・削除は保留されます。
	 */
	virtual void Update();

	/**
	 * @brief 3D描画処理
	 * @param camera カメラ管理クラス
	 */
	virtual void Draw3D(CameraManager* camera);

	/**
	 * @brief 2D描画処理
	 */
	virtual void Draw2D();

	/**
	 * @brief シャドウマップへの描画処理
	 */
	virtual void DrawShadow();
	virtual void DrawGBuffer();

	/**
	 * @brief 子オブジェクトのリストを取得
	 * @return 子オブジェクトのマップ
	 */
	const std::unordered_map<std::string, std::unique_ptr<GameObject>>& GetChildren() const { return children_; }

	/**
	 * @brief トランスフォーム情報の更新
	 * @param camera カメラ管理クラス
	 */
	void UpdateTransform(CameraManager* camera);

	/**
	 * @brief コンポーネントの追加
	 * 
	 * 指定された名前でコンポーネントを追加します。
	 * 更新中の場合は保留リストに追加され、更新終了後に実際に追加されます。
	 * 
	 * @param name コンポーネント名（一意識別子）
	 * @param comp 追加するコンポーネント
	 */
	void AddComponent(const std::string& name, std::unique_ptr<GameObjectComponent::IGameObjectComponent> comp);

	/**
	 * @brief コンポーネントの削除
	 * 
	 * 指定された名前のコンポーネントを削除します。
	 * 更新中の場合は保留リストに追加され、更新終了後に実際に削除されます。
	 * 
	 * @param name 削除するコンポーネント名
	 */
	void RemoveComponent(const std::string& name);

	/**
	 * @brief 型指定でのコンポーネント取得
	 * 
	 * @tparam T 取得するコンポーネントの型
	 * @return 指定された型のコンポーネント（見つからない場合はnullptr）
	 */
	template<typename T>
	std::shared_ptr<T> GetComponent() const;

public: // アクセッサ
	// === Transform関連 ===
	/**
	 * @brief 位置の設定
	 * @param pos 設定する位置ベクトル
	 */
	virtual void SetPosition(const Vector3& pos) { transform_.translate = pos; }

	/**
	 * @brief 回転の設定
	 * @param rot 設定する回転ベクトル（オイラー角）
	 */
	virtual void SetRotation(const Vector3& rot) { transform_.rotate = rot; }

	/**
	 * @brief スケールの設定
	 * @param scale 設定するスケールベクトル
	 */
	virtual void SetScale(const Vector3& scale) { transform_.scale = scale; }

	/**
	 * @brief 位置の取得
	 * @return 現在の位置ベクトル
	 */
	virtual const Vector3& GetPosition() const { return transform_.translate; }

	/**
	 * @brief 回転の取得
	 * @return 現在の回転ベクトル（オイラー角）
	 */
	virtual const Vector3& GetRotation() const { return transform_.rotate; }

	/**
	 * @brief スケールの取得
	 * @return 現在のスケールベクトル
	 */
	virtual const Vector3& GetScale() const { return transform_.scale; }

	/**
	 * @brief ワールド行列の更新
	 * 
	 * ローカルトランスフォームから親子関係を考慮したワールド行列を計算します。
	 */
	void UpdateWorldMatrix();

	/**
	 * @brief ワールド行列の取得
	 * @return 現在のワールド行列
	 */
	Matrix4x4 GetWorldMatrix() const { return renderable3d_ ? renderable3d_->GetWorldMatrix() : MakeIdentity4x4(); }

	// === 3Dオブジェクト関連 ===
	/**
	 * @brief 3Dモデルの設定
	 * @param modelName 設定するモデル名
	 */
	void SetModel(const std::string& modelName);

	/**
	 * @brief スキニングモデルの設定
	 * @param modelPath モデルのファイルパス
	 * @param ext モデルファイルの拡張子（デフォルト: ".gltf"）
	 */
	void SetSkinnedModel(const std::string& modelPath, const std::string& ext = ".gltf");

	/**
	 * @brief 3Dモデルの取得
	 * @return 現在のモデル
	 */
	Model* GetModel() const;

	/**
	 * @brief Object3Dインスタンスの取得
	 * @return Object3Dのポインタ
	 */
	IRenderable3d* GetRenderable3d() const { return renderable3d_.get(); }

	/**
	 * @brief Object3Dインスタンスの取得（互換用）
	 * @return Object3Dのポインタ（静的モデルの場合のみ有効）
	 */
	Object3d* GetObject3d() const { return dynamic_cast<Object3d*>(renderable3d_.get()); }

	/**
	 * @brief SkinnedObject3Dインスタンスの取得
	 * @return SkinnedObject3Dのポインタ（スキニングモデルの場合のみ有効）
	 */
	SkinnedObject3d* GetSkinnedObject3d() const { return dynamic_cast<SkinnedObject3d*>(renderable3d_.get()); }

	/**
	 * @brief オブジェクトの色を設定
	 * @param color 設定する色（RGBA）
	 */
	void SetColor(const Vector4& color) { if (renderable3d_) renderable3d_->SetColor(color); }

	/**
	 * @brief オブジェクトの色を取得
	 * @return 現在の色（RGBA）
	 */
	Vector4 GetColor() const { return renderable3d_ ? renderable3d_->GetColor() : Vector4(1,1,1,1); }

	// === タグ関連 ===
	/**
	 * @brief オブジェクトタグの取得
	 * @return 現在のタグ文字列
	 */
	std::string GetTag() const { return tag_; }

	/**
	 * @brief オブジェクトタグの設定
	 * @param tag 設定するタグ文字列
	 */
	void SetTag(const std::string& tag) { tag_ = tag; }

	// === 名前関連 ===
	/**
	 * @brief オブジェクト名の設定
	 * @param name 設定する名前文字列
	 */
	void SetName(const std::string& name) { name_ = name; }

	/**
	 * @brief オブジェクト名の取得
	 * @return 現在の名前文字列
	 */
	std::string GetName() const { return name_; }

	// === アクティブ状態関連 ===
	/**
	 * @brief アクティブ状態の設定
	 * @param isActive 設定するアクティブ状態
	 */
	void SetActive(bool isActive) { isActive_ = isActive; }

	/**
	 * @brief アクティブ状態の取得
	 * @return 現在のアクティブ状態
	 */
	bool IsActive() const { return isActive_; }

	// === 親子関係関連 ===
	/**
	 * @brief 子オブジェクトの追加
	 * 
	 * @param name 子オブジェクトの名前（一意識別子）
	 * @param child 追加する子オブジェクト
	 */
	void AddChild(const std::string& name, std::unique_ptr<GameObject> child);

	/**
	 * @brief 子オブジェクトの取得
	 * @param name 取得する子オブジェクトの名前
	 * @return 指定された名前の子オブジェクト（見つからない場合はnullptr）
	 */
	GameObject* GetChild(const std::string& name) const;

protected:
	// オブジェクトのトランスフォーム情報（位置、回転、スケール）
	Transform transform_;
	// 3D描画用オブジェクト（Object3dまたはSkinnedObject3d）
	std::unique_ptr<IRenderable3d> renderable3d_;
	// Object3dCommonへのポインタ（SetSkinnedModel用）
	Object3dCommon* object3dCommon_ = nullptr;
	// LightManagerへのポインタ（SetSkinnedModel用）
	LightManager* lightManager_ = nullptr;

private:
	/**
	 * @brief Transform情報をObject3Dに適用
	 * @param camera カメラ管理クラス
	 */
	void ApplyTransformToObject3D(CameraManager* camera);

	/**
	 * @brief 親オブジェクトの設定
	 * @param parent 設定する親オブジェクト
	 */
	void SetParent(GameObject* parent) { parent_ = parent; }

	/**
	 * @brief ImGuiでの階層表示
	 */
	void ShowImGuiHierarchy();

	/**
	 * @brief コンポーネントの即座追加
	 * @param name コンポーネント名
	 * @param comp 追加するコンポーネント
	 */
	void AddComponentImmediate(const std::string& name, std::shared_ptr<GameObjectComponent::IGameObjectComponent> comp);

	/**
	 * @brief コンポーネントの即座削除
	 * @param name 削除するコンポーネント名
	 */
	void RemoveComponentImmediate(const std::string& name);

	/**
	 * @brief カテゴリリストからコンポーネントを削除
	 * @param comp 削除するコンポーネント
	 */
	void RemoveFromCategoryLists(const std::shared_ptr<GameObjectComponent::IGameObjectComponent>& comp);

	/**
	 * @brief 保留中の変更処理
	 */
	void ProcessPendingChanges();

private:
	// === コンポーネントシステム ===
	// 全コンポーネントのマップ
	std::unordered_map<std::string, std::shared_ptr<GameObjectComponent::IGameObjectComponent>> components_;
	// アクションコンポーネントのリスト（高速アクセス用）
	std::vector<std::shared_ptr<GameObjectComponent::IActionComponent>> actionComponents_;
	// コリジョンコンポーネントのリスト（高速アクセス用）
	std::vector<std::shared_ptr<GameObjectComponent::ICollisionComponent>> collisionComponents_;

	// === 保留処理システム ===
	// 追加保留中のコンポーネントリスト
	std::vector<std::pair<std::string, std::shared_ptr<GameObjectComponent::IGameObjectComponent>>> pendingAdds_;
	// 削除保留中のコンポーネント名リスト
	std::vector<std::string> pendingRemoves_;
	// 更新処理中フラグ
	bool isUpdating_ = false;

	// === オブジェクト基本情報 ===
	// オブジェクトのタグ（分類用）
	std::string tag_;
	// オブジェクトの名前（識別用）
	std::string name_ = "";
	// アクティブ状態フラグ
	bool isActive_;

	// === 親子関係システム ===
	// 子オブジェクトのマップ
	std::unordered_map<std::string, std::unique_ptr<GameObject>> children_;
	// 親オブジェクトへのポインタ
	GameObject* parent_ = nullptr;
};

/**
 * @brief 型指定でのコンポーネント取得（テンプレート実装）
 * 
 * @tparam T 取得するコンポーネントの型
 * @return 指定された型のコンポーネント（見つからない場合はnullptr）
 */
template <typename T>
std::shared_ptr<T> GameObject::GetComponent() const
{
	// 全コンポーネントを走査
	for (const auto& [_, comp] : components_)
	{
		// 指定された型へのキャストを試行
		if (auto casted = std::dynamic_pointer_cast<T>(comp))
		{
			return casted;
		}
	}
	return nullptr;
}