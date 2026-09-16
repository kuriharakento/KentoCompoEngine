#pragma once
#include <memory>
#include <string>
#include <string_view>
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
#include "engine/gameobject/component/base/Behaviour.h"
#include "engine/gameobject/component/base/Collider.h"
#include "engine/gameobject/component/base/ComponentFactory.h"
// json
#include "jsonEditor/JsonEditableBase.h"
// core
#include "core/Guid.h"
// time
#include "time/ClockId.h"
// graphics
#include "graphics/view/RenderLayer.h"

namespace KCE
{
namespace GameObjectComponent
{
	class Component;
	class Behaviour;
	class Collider;
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
class GameObject : public JsonEditableBase
{
public:
	/**
	 * @brief JSONデータの読み込み（オーバーライド）
	 */
	bool LoadJson(const std::string& path) override;

	/**
	 * @brief JSONデータの保存（オーバーライド）
	 */
	bool SaveJson(const std::string& path) const override;

	/**
	 * @brief このオブジェクトと子をプレハブとして保存する。
	 * @param path application/Resources/json/prefab からの相対パス
	 * @return 保存できたら真
	 */
	bool SavePrefab(const std::string& path) const;

	/** @brief GUIDを含まないプレハブ形式へ再帰的に変換する。 */
	nlohmann::json SerializePrefab() const;

	/**
	 * @brief ImGuiによる編集UIを描画（オーバーライド）
	 */
	void DrawImGui() override;
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
	/** @brief 全オブジェクトの Update 後にコンポーネントを更新する。 */
	virtual void LateUpdate();

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
	 * @param camera 使用するカメラ（省略時はデフォルトカメラを使用）
	 */
	virtual void DrawShadow(Camera* camera = nullptr);
	virtual void DrawGBuffer(CameraManager* camera = nullptr);

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
	GameObjectComponent::Component* AddComponent(std::unique_ptr<GameObjectComponent::Component> comp, const std::string& typeName);

	template<typename T, typename... Args>
	T* AddComponent(Args&&... args);

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
	T* GetComponent() const;

	template<typename T>
	void GetComponents(std::vector<T*>& out) const;
	/** @brief 有効な全コンポーネントへ組単位の Enter を送る。 */
	void DispatchCollisionEnter(const GameObjectComponent::CollisionInfo& info);
	/** @brief 有効な全コンポーネントへ組単位の Stay を送る。 */
	void DispatchCollisionStay(const GameObjectComponent::CollisionInfo& info);
	/** @brief 有効な全コンポーネントへ組単位の Exit を送る。 */
	void DispatchCollisionExit(const GameObjectComponent::CollisionInfo& info);
	/** @brief 有効な全コンポーネントへオブジェクト単位の Enter を送る。 */
	void DispatchObjectCollisionEnter(const GameObjectComponent::CollisionInfo& info);
	/** @brief 有効な全コンポーネントへオブジェクト単位の Stay を送る。 */
	void DispatchObjectCollisionStay(const GameObjectComponent::CollisionInfo& info);
	/** @brief 有効な全コンポーネントへオブジェクト単位の Exit を送る。 */
	void DispatchObjectCollisionExit(const GameObjectComponent::CollisionInfo& info);

	/**
	 * @brief 名前と型を指定してのコンポーネント取得
	 *
	 * @tparam T 取得するコンポーネントの型
	 * @param name コンポーネント名（一意識別子）
	 * @return 指定された型と名前のコンポーネント（見つからない場合はnullptr）
	 */

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
	/** @return 静的モデルの設定名。モデルを持たなければ空文字列。 */
	const std::string& GetModelName() const { return modelName_; }

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

	// === GUID関連 ===
	/**
	 * @brief 安定IDの取得
	 * @details 演出データなど外部からの参照は、名前ではなくこのGUIDで行う。
	 *          名前は改名・重複で壊れるため表示専用とする。
	 * @return このオブジェクトのGUID
	 */
	const Guid& GetGuid() const { return guid_; }

	/**
	 * @brief 安定IDの設定
	 * @details シーンのロード時に、保存されていたGUIDを復元するために使う。
	 *          通常の生成時はコンストラクタが自動採番するので呼ぶ必要はない。
	 * @param guid 復元するGUID
	 */
	void SetGuid(const Guid& guid) { guid_ = guid; }

	// === 描画レイヤー関連 ===
	/**
	 * @brief 描画レイヤーを上書きする
	 * @details 既定レイヤーからも外れる。今の所属を残す場合は AddRenderLayer を使う。
	 *          0にするとどのビューにも描かれない。子の GameObject には伝えない。
	 * @param layer 新しいレイヤーマスク
	 */
	void SetRenderLayer(RenderLayerMask layer) { renderLayer_ = layer; }

	/**
	 * @brief 今の所属を残して描画レイヤーを足す
	 * @param layer 足すレイヤーマスク
	 */
	void AddRenderLayer(RenderLayerMask layer) { renderLayer_ |= layer; }

	/**
	 * @brief 指定した描画レイヤーを外す
	 * @details 全部外れて0になると、どのビューにも描かれない。子の GameObject には伝えない。
	 * @param layer 外すレイヤーマスク
	 */
	void RemoveRenderLayer(RenderLayerMask layer) { renderLayer_ &= ~layer; }

	/**
	 * @brief 指定した描画レイヤーのどれかに入っているか調べる
	 * @param layer 調べるレイヤーマスク
	 * @return 1つ以上の所属が一致すれば真
	 */
	bool IsInRenderLayer(RenderLayerMask layer) const { return (renderLayer_ & layer) != 0; }

	/**
	 * @brief 描画レイヤーの取得
	 * @return 所属レイヤー
	 */
	RenderLayerMask GetRenderLayer() const { return renderLayer_; }

	// === アクティブ状態関連 ===
	/**
	 * @brief アクティブ状態の設定
	 * @param isActive 設定するアクティブ状態
	 */
	void SetActive(bool isActive);
	/** @brief enabled 変更後に寿命通知を同期する。 */
	void RefreshComponentActivation() { SyncComponentActivation(); }

	/**
	 * @brief アクティブ状態の取得
	 * @return 現在のアクティブ状態
	 */
	bool IsActive() const { return isActive_ && (!parent_ || parent_->IsActive()); }
	/** @brief コンポーネントを破棄している途中か返す。 */
	bool IsDestroying() const { return isDestroying_; }

	/**
	 * @brief オブジェクトの破棄を要求する（フレーム末尾で安全にメモリ解放されます）
	 */
	void Destroy() { isPendingDestroy_ = true; }

	/**
	 * @brief 破棄保留中であるかを取得
	 */
	bool IsPendingDestroy() const { return isPendingDestroy_; }

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

	/**
	 * @brief 親オブジェクトを取得する。
	 * @return 親オブジェクト（所有しない）。ルートの場合はnullptr。
	 */
	GameObject* GetParent() const { return parent_; }

	/**
	 * @brief この GameObject（と子・コンポーネント）が使う時計を決める
	 * @param clock TimeManager::CreateClock で作った時計。指定なしに戻すと親の時計を使う
	 */
	void SetClock(ClockId clock) { clock_ = clock; }

	/**
	 * @brief 名前で使う時計を決める。ここで1回だけ探して番号で覚える
	 * @param clockName 時計の名前。見つからなければ警告を出し、指定なし（親の時計）に戻す。時計は起動時に作っておく
	 */
	void SetClock(std::string_view clockName);

	/**
	 * @brief 使う時計を返す
	 * @return 自分に指定があればそれ、無ければ（か消えていれば）親の時計、親もいなければ Game
	 */
	ClockId GetClock() const;

	/** @brief 使う時計の、倍率を掛けた1フレームの経過時間 */
	float GetDeltaTime() const;

	/**
	 * @brief アタッチされている全コンポーネントを取得
	 */
	const std::vector<std::unique_ptr<GameObjectComponent::Component>>& GetComponents() const { return components_; }
	/** @brief コンポーネントの保存名を追加順で返す。 */
	const std::vector<std::string>& GetComponentTypeNames() const { return componentTypeNames_; }

	/**
	 * @brief 内部で使用している Object3dCommon を取得
	 */
	Object3dCommon* GetObject3dCommon() const { return object3dCommon_; }

	/**
	 * @brief 内部で使用している LightManager を取得
	 */
	LightManager* GetLightManager() const { return lightManager_; }

protected:
	// オブジェクトのトランスフォーム情報（位置、回転、スケール）
	Transform transform_;
	// 3D描画用オブジェクト（Object3dまたはSkinnedObject3d）
	std::unique_ptr<IRenderable3d> renderable3d_;
	// SetModel で設定した静的モデル名。プレハブ保存時に使う
	std::string modelName_;
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
	GameObjectComponent::Component* AddComponentImmediate(std::unique_ptr<GameObjectComponent::Component> comp, const std::string& typeName);

	/**
	 * @brief コンポーネントの即座削除
	 * @param name 削除するコンポーネント名
	 */
	void RemoveComponentImmediate(const std::string& name);

	/**
	 * @brief カテゴリリストからコンポーネントを削除
	 * @param comp 削除するコンポーネント
	 */
	/**
	 * @brief 保留中の変更処理
	 */
	void ProcessPendingChanges();
	void SyncComponentActivation();
	void DestroyAllComponents();

private:
	// === コンポーネントシステム ===
	// 全コンポーネントのマップ
	std::vector<std::unique_ptr<GameObjectComponent::Component>> components_;
	std::vector<std::string> componentTypeNames_;

	// === 保留処理システム ===
	// 追加保留中のコンポーネントリスト
	std::vector<std::pair<std::string, std::unique_ptr<GameObjectComponent::Component>>> pendingAdds_;
	// 削除保留中のコンポーネント名リスト
	std::vector<std::string> pendingRemoves_;
	// 更新処理中フラグ
	bool isUpdating_ = false;
	bool isDestroying_ = false;

	// === オブジェクト基本情報 ===
	// オブジェクトのタグ（分類用）
	std::string tag_;
	// オブジェクトの名前（識別用・表示専用）
	std::string name_ = "";
	// 安定ID。生成時に自動採番し、シーンのロード時のみ保存値で上書きする
	Guid guid_ = Guid::Generate();
	// 所属する描画レイヤーの集合。ビューごとの描き分けに使う
	RenderLayerMask renderLayer_ = kRenderLayerDefault;
	// アクティブ状態フラグ
	bool isActive_;
	// 破棄保留中フラグ
	bool isPendingDestroy_ = false;

	// === 親子関係システム ===
	// 子オブジェクトのマップ
	std::unordered_map<std::string, std::unique_ptr<GameObject>> children_;
	// 親オブジェクトへのポインタ
	GameObject* parent_ = nullptr;

	// 使う時計。指定なしなら親の時計（それも無ければ Game）
	ClockId clock_{};
};

/**
 * @brief 型指定でのコンポーネント取得（テンプレート実装）
 *
 * @tparam T 取得するコンポーネントの型
 * @return 指定された型のコンポーネント（見つからない場合はnullptr）
 */
template <typename T>
T* GameObject::GetComponent() const
{
	for (const auto& component : components_)
	{
		if (auto* result = dynamic_cast<T*>(component.get()))
		{
			return result;
		}
	}
	return nullptr;
}

template<typename T, typename... Args>
T* GameObject::AddComponent(Args&&... args)
{
	auto component = std::make_unique<T>(std::forward<Args>(args)...);
	T* result = component.get();
	AddComponent(std::move(component), GameObjectComponent::ComponentFactory::GetInstance()->GetTypeName(typeid(T)));
	return result;
}

template<typename T>
void GameObject::GetComponents(std::vector<T*>& out) const
{
	out.clear();
	for (const auto& component : components_)
	{
		if (auto* result = dynamic_cast<T*>(component.get()))
		{
			out.push_back(result);
		}
	}
}
} // namespace KCE
