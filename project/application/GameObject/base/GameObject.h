#pragma once
#include <memory>
#include <string>
#include <algorithm>

// graphics
#include "graphics/3d/Object3d.h"
// math
#include "base/GraphicsTypes.h"
// component
#include "GameObjectTag.h"
#include "application/GameObject/component/base/IActionComponent.h"
#include "application/GameObject/component/base/ICollisionComponent.h"

/**
 * @class GameObject
 * @brief ゲーム内の全てのオブジェクトの基底クラス
 *
 * 3D空間でのトランスフォーム、描画、コンポーネントシステム、親子関係を管理する。
 * Entity-Component-System(ECS)パターンの一部として設計されており、
 * 様々なコンポーネントを動的に追加・削除することで機能を拡張できる。
 */
class GameObject
{
public:
	/**
	 * @brief デストラクタ
	 *
	 * 全てのコンポーネントをクリアし、オブジェクトを非アクティブ状態にする
	 */
	virtual ~GameObject();

	/**
	 * @brief コンストラクタ
	 * @param tag オブジェクトのタグ（デフォルト: GameObjectTag::Common::GameObject）
	 *
	 * GameObjectを初期化し、指定されたタグを設定する
	 */
	explicit GameObject(std::string tag = GameObjectTag::Common::GameObject);

	/**
	 * @brief GameObjectの初期化
	 * @param object3dCommon 3Dオブジェクト共通データ
	 * @param lightManager ライト管理クラス
	 * @param camera カメラオブジェクト（オプション）
	 *
	 * 3Dオブジェクトの初期化とデフォルトトランスフォームの設定を行う
	 */
	virtual void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, const Transform& initialTransform = Transform());

	/**
	 * @brief 毎フレームの更新処理
	 *
	 * アクションコンポーネント、ワールド行列、コリジョンコンポーネント、
	 * 子オブジェクトの更新を順次実行する
	 */
	virtual void Update();

	/**
	 * @brief 描画処理
	 * @param camera カメラ管理クラス
	 *
	 * 自身と子オブジェクト、アクションコンポーネントの描画を行う
	 */
	virtual void Draw(CameraManager* camera);

	/**
	 * @brief トランスフォーム情報の更新
	 * @param camera カメラ管理クラス
	 *
	 * Transform情報をObject3Dに適用する
	 */
	void UpdateTransform(CameraManager* camera);

	/**
	 * @brief コンポーネントの追加
	 * @param name コンポーネント名（一意識別子）
	 * @param comp 追加するコンポーネント
	 *
	 * 指定された名前でコンポーネントを追加する。
	 * 更新中の場合は保留リストに追加され、更新終了後に実際に追加される
	 */
	void AddComponent(const std::string& name, std::unique_ptr<IGameObjectComponent> comp);

	/**
	 * @brief コンポーネントの削除
	 * @param name 削除するコンポーネント名
	 *
	 * 指定された名前のコンポーネントを削除する。
	 * 更新中の場合は保留リストに追加され、更新終了後に実際に削除される
	 */
	void RemoveComponent(const std::string& name);

	/**
	 * @brief 型指定でのコンポーネント取得
	 * @tparam T 取得するコンポーネントの型
	 * @return 指定された型のコンポーネント（見つからない場合はnullptr）
	 *
	 * テンプレートを使用して指定された型のコンポーネントを検索・取得する
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
	 * @brief 現在の位置を取得
	 * @return 現在の位置ベクトル
	 */
	virtual const Vector3& GetPosition() const { return transform_.translate; }

	/**
	 * @brief 現在の回転を取得
	 * @return 現在の回転ベクトル（オイラー角）
	 */
	virtual const Vector3& GetRotation() const { return transform_.rotate; }

	/**
	 * @brief 現在のスケールを取得
	 * @return 現在のスケールベクトル
	 */
	virtual const Vector3& GetScale() const { return transform_.scale; }

	/**
	 * @brief ワールド行列の更新
	 *
	 * ローカルトランスフォームから親子関係を考慮したワールド行列を計算し、
	 * 子オブジェクトにも伝播させる
	 */
	void UpdateWorldMatrix();

	/**
	 * @brief ワールド行列の取得
	 * @return 現在のワールド行列（Object3Dが無い場合は単位行列）
	 */
	Matrix4x4 GetWorldMatrix() const { return object3d_ ? object3d_->GetWorldMatrix() : MakeIdentity4x4(); }

	// === 3Dオブジェクト関連 ===
	/**
	 * @brief 3Dモデルの設定
	 * @param modelName 設定するモデル名
	 */
	void SetModel(const std::string& modelName) { object3d_->SetModel(modelName); }

	/**
	 * @brief 3Dモデルの取得
	 * @return 現在のモデル（Object3Dが無い場合はnullptr）
	 */
	Model* GetModel() const { return object3d_ ? object3d_->GetModel() : nullptr; }

	/**
	 * @brief Object3Dインスタンスの取得
	 * @return Object3Dのポインタ
	 */
	Object3d* GetObject3d() const { return object3d_.get(); }

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
	 * @param name 子オブジェクトの名前（一意識別子）
	 * @param child 追加する子オブジェクト
	 *
	 * 指定された名前で子オブジェクトを追加し、親子関係を設定する
	 */
	void AddChild(const std::string name, std::unique_ptr<GameObject> child);

	/**
	 * @brief 子オブジェクトの取得
	 * @param name 取得する子オブジェクトの名前
	 * @return 指定された名前の子オブジェクト（見つからない場合はnullptr）
	 */
	GameObject* GetChild(const std::string& name) const;

protected:
	Transform transform_;					///< オブジェクトのトランスフォーム情報（位置、回転、スケール）
	std::unique_ptr<Object3d> object3d_;	///< 3D描画用オブジェクト

private:
	/**
	 * @brief Transform情報をObject3Dに適用
	 * @param camera カメラ管理クラス
	 *
	 * 親子関係を考慮してワールド行列を計算し、Object3Dに設定する
	 */
	void ApplyTransformToObject3D(CameraManager* camera);

	/**
	 * @brief 親オブジェクトの設定
	 * @param parent 設定する親オブジェクト
	 *
	 * 内部的に使用される親子関係設定用の関数
	 */
	void SetParent(GameObject* parent) { parent_ = parent; }

	/**
	 * @brief ImGuiでの階層表示
	 *
	 * デバッグ用のImGui階層ビューを表示する
	 */
	void ShowImGuiHierarchy();

	/**
	 * @brief コンポーネントの即座追加
	 * @param name コンポーネント名
	 * @param comp 追加するコンポーネント
	 *
	 * 更新処理に関係なく即座にコンポーネントを追加する内部関数
	 */
	void AddComponentImmediate(const std::string& name, std::shared_ptr<IGameObjectComponent> comp);

	/**
	 * @brief コンポーネントの即座削除
	 * @param name 削除するコンポーネント名
	 *
	 * 更新処理に関係なく即座にコンポーネントを削除する内部関数
	 */
	void RemoveComponentImmediate(const std::string& name);

	/**
	 * @brief カテゴリリストからコンポーネントを削除
	 * @param comp 削除するコンポーネント
	 *
	 * アクション・コリジョンコンポーネントのカテゴリ配列から指定コンポーネントを削除
	 */
	void RemoveFromCategoryLists(const std::shared_ptr<IGameObjectComponent>& comp);

	/**
	 * @brief 保留中の変更処理
	 *
	 * 更新中に保留されたコンポーネントの追加・削除を実行する
	 */
	void ProcessPendingChanges();

private:
	// === コンポーネントシステム ===
	std::unordered_map<std::string, std::shared_ptr<IGameObjectComponent>> components_;		///< 全コンポーネントのマップ
	std::vector<std::shared_ptr<IActionComponent>> actionComponents_;						///< アクションコンポーネントのリスト（高速アクセス用）
	std::vector<std::shared_ptr<ICollisionComponent>> collisionComponents_;				///< コリジョンコンポーネントのリスト（高速アクセス用）

	// === 保留処理システム ===
	std::vector<std::pair<std::string, std::shared_ptr<IGameObjectComponent>>> pendingAdds_;	///< 追加保留中のコンポーネントリスト
	std::vector<std::string> pendingRemoves_;													///< 削除保留中のコンポーネント名リスト
	bool isUpdating_ = false;																	///< 更新処理中フラグ

	// === オブジェクト基本情報 ===
	std::string tag_;						///< オブジェクトのタグ（分類用）
	std::string name_ = "";					///< オブジェクトの名前（識別用）
	bool isActive_;							///< アクティブ状態フラグ

	// === 親子関係システム ===
	std::unordered_map<std::string, std::unique_ptr<GameObject>> children_;	///< 子オブジェクトのマップ
	GameObject* parent_ = nullptr;												///< 親オブジェクトへのポインタ
};

/**
 * @brief 型指定でのコンポーネント取得（テンプレート実装）
 * @tparam T 取得するコンポーネントの型
 * @return 指定された型のコンポーネント（見つからない場合はnullptr）
 *
 * 全コンポーネントを順次チェックし、指定された型にキャスト可能な
 * 最初のコンポーネントを返す
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
			return casted;	// キャスト成功した場合はそのコンポーネントを返す
		}
	}
	return nullptr;	// 該当するコンポーネントが見つからない場合
}