#pragma once
#include <vector>
#include <string>
#include <memory>
#include <unordered_set>

#include "core/Guid.h"
#include "gameobject/manager/GameObjectRenderer.h"
#include "graphics/view/RenderLayer.h"
#include "base/GraphicsTypes.h"

namespace KCE
{
class GameObject;
class CameraManager;
class Camera;
class Object3dCommon;
class Object3d;
class IRenderable3d;
class LightManager;

/**
 * @brief GameObjectを一括管理するマネージャークラス（シングルトン）
 * 
 * 登録されたすべてのGameObjectに対して、Update、Draw3D、DrawShadow、DrawGBuffer、Draw2Dを
 * 自動的に回します。また、名前指定での検索機能を提供します。
 */
class GameObjectManager
{
public:
	/**
	 * @brief シングルトンインスタンスの取得
	 * @return GameObjectManager* インスタンス
	 */
	static GameObjectManager* GetInstance();
	static bool HasInstance();

	/**
	 * @brief 初期化
	 */
	void Initialize();

	/**
	 * @brief 終了処理
	 */
	void Finalize();

	/**
	 * @brief GameObjectの登録
	 * @param gameObject 登録するオブジェクト
	 */
	void Register(GameObject* gameObject);

	/**
	 * @brief GameObjectの登録解除
	 * @param gameObject 登録解除するオブジェクト
	 */
	void Unregister(GameObject* gameObject);

	/**
	 * @brief すべてのGameObjectの更新
	 */
	void Update();

	/**
	 * @brief 3D描画
	 * @param camera カメラ管理クラス
	 */
	void Draw3D(CameraManager* camera);

	/** @brief 現在のビューに見える半透明を距離の降順で描く */
	void DrawTransparent(CameraManager* camera, const std::vector<Object3d*>& sceneObjects = {});

	/**
	 * @brief 2D描画
	 */
	void Draw2D();

	/**
	 * @brief シャドウマップ描画
	 * @param camera 使用するカメラ
	 */
	void DrawShadow(Camera* camera = nullptr);

	/**
	 * @brief G-Buffer描画
	 * @param camera カメラ管理クラス
	 */
	void DrawGBuffer(CameraManager* camera = nullptr);

	/**
	 * @brief このフレームの描画に使う行列を、登録された GameObject とその子の全員分まとめて確定させる
	 * @details 描画パイプラインの頭で1回呼ぶ（Framework::ExecuteRenderPipeline）。
	 *          これまでは Draw3D・DrawGBuffer・影・モニター・反射と、ビューを描くたびに同じ行列を計算し直していた
	 */
	void UpdateRenderTransforms();

	/**
	 * @brief 動的なGameObjectの作成
	 * @param name オブジェクト名
	 * @param tag タグ
	 * @return 生成されたオブジェクトのポインタ（マネージャーが所有）
	 */
	GameObject* CreateGameObject(const std::string& name = "GameObject", const std::string& tag = "GameObject");

	/**
	 * @brief プレハブを読み込み、まだmanagerへ登録していないツリーを返す。
	 * @param prefabPath application/Resources/json/prefab からの相対パス
	 * @return 読み込みに成功したツリー。所有権は呼び出し側へ移る
	 */
	std::unique_ptr<GameObject> LoadPrefab(const std::string& prefabPath) const;

	/**
	 * @brief プレハブを生成してmanager所有にする。
	 * @param prefabPath application/Resources/json/prefab からの相対パス
	 * @param transform 生成するrootへ適用するローカルtransform
	 * @return 生成したroot。所有権はmanagerが持つ。失敗時はnullptr
	 */
	GameObject* Instantiate(const std::string& prefabPath, const Transform& transform = Transform());

	/**
	 * @brief 名前でGameObjectを検索
	 * @param name 検索する名前
	 * @return 最初に見つかったGameObject。無ければnullptr
	 */
	GameObject* Find(const std::string& name) const;

	/**
	 * @brief GUIDでGameObjectを検索
	 * @details 演出データからの参照はこちらを使う。名前引きと違い、改名や
	 *          同名オブジェクトの追加で参照が壊れることがない。
	 * @param guid 検索するGUID
	 * @return 見つかったGameObject。無ければnullptr
	 */
	GameObject* FindByGuid(const Guid& guid) const;

	/**
	 * @brief 指定した名前のGameObjectをすべて検索
	 * @param name 検索する名前
	 * @return 見つかったGameObjectのリスト
	 */
	std::vector<GameObject*> FindAll(const std::string& name) const;

	/**
	 * @brief タグでGameObjectを検索
	 * @param tag 検索するタグ
	 * @return 最初に見つかったGameObject。無ければnullptr
	 */
	GameObject* FindWithTag(const std::string& tag) const;

	/**
	 * @brief 指定したタグのGameObjectをすべて検索
	 * @param tag 検索するタグ
	 * @return 見つかったGameObjectのリスト
	 */
	std::vector<GameObject*> FindAllWithTag(const std::string& tag) const;

	/**
	 * @brief 管理中の全GameObjectを取得
	 */
	const std::vector<GameObject*>& GetGameObjects() const { return gameObjects_; }

	/**
	 * @brief これから描くビューのレイヤーマスクを設定する
	 *
	 * @details Draw3D / DrawGBuffer は、このマスクに合致するオブジェクトだけを描く。
	 *          描画経路は SceneManager からアプリのシーンを経由するため、
	 *          引数で引き回すにはアプリ側のシーンを全て書き換える必要がある。
	 *          ビューの描画は必ずパイプラインが順番に行うので、
	 *          描画の直前に設定して直後に戻す運用にしている。
	 *
	 * @param mask 描画対象のレイヤー集合
	 */
	void SetRenderLayerMask(RenderLayerMask mask) { renderLayerMask_ = mask; }

	/**
	 * @brief 現在の描画対象レイヤーマスク
	 * @return レイヤーマスク
	 */
	RenderLayerMask GetRenderLayerMask() const { return renderLayerMask_; }

	/**
	 * @brief 影を描く間だけ、視錐台カリングに使うライトのビュー×プロジェクション行列を設定する
	 * @details 影は SceneManager からアプリのシーンを経由して描くので、レイヤーマスクと同じく
	 *          描く直前に設定して直後に nullptr へ戻す。nullptr の間は影を省かない（ポイントライトなど）
	 * @param viewProjection ライトの行列。描き終わるまで生きていること（所有しない）
	 */
	void SetShadowCullingViewProjection(const Matrix4x4* viewProjection) { shadowCullingViewProjection_ = viewProjection; }

	/**
	 * @brief 今描いているビュー（または影）で、この描画物が見えるか
	 * @details モデルの AABB をワールドへ移し、8つの角を行列でクリップ空間へ送って、全部が同じ面の外なら見えないとする。
	 *          AABB を持たない描画物、判定する行列が無いとき、カリングを切っているときは常に真。
	 *          見えた数・省いた数はフレームごとに数えて、描画の計測ページに出す
	 * @param renderable 描画物。ワールド行列はこのビュー用に確定している前提
	 * @return 描くなら真
	 */
	bool IsRenderableVisible(const IRenderable3d* renderable);

	/** @brief 視錐台カリングを使うか（比べるときに切る） */
	void SetCullingEnabled(bool enabled) { renderer_.SetCullingEnabled(enabled); }
	bool IsCullingEnabled() const { return renderer_.IsCullingEnabled(); }

	/** @brief 前のフレームで GameObject を描いた数（全ビューと影の合計） */
	uint32_t GetLastFrameDrawnCount() const { return renderer_.GetLastFrameDrawnCount(); }
	/** @brief 前のフレームで視錐台の外として省いた数（全ビューと影の合計） */
	uint32_t GetLastFrameCulledCount() const { return renderer_.GetLastFrameCulledCount(); }

public:
	~GameObjectManager() = default;

private:
	friend std::unique_ptr<GameObjectManager> std::make_unique<GameObjectManager>();
	GameObjectManager() = default;
	GameObjectManager(const GameObjectManager&) = delete;
	GameObjectManager& operator=(const GameObjectManager&) = delete;

	/**
	 * @brief 破棄保留中のオブジェクトを安全に解放・登録解除する
	 */
	void ClearPendingDestroyObjects();

private:
	static std::unique_ptr<GameObjectManager> instance_;

	// 管理しているGameObjectのリスト（非所有ポインタ）
	std::vector<GameObject*> gameObjects_;
	// 更新開始時の登録一覧。更新中の追加・削除で反復が壊れないよう、確保領域を使い回す
	std::vector<GameObject*> updateObjects_;
	// 登録解除で本体が破棄されても、ポインタを逆参照せず登録中か判定する
	std::unordered_set<GameObject*> registeredObjects_;

	// 現在描いているビューの描画対象レイヤー
	RenderLayerMask renderLayerMask_ = kRenderLayerAll;

	// 今描いているビューのビュー×プロジェクション。GameObject を直接描く経路の判定だけに使う（カメラが持つ行列。所有しない）
	const Matrix4x4* viewCullingViewProjection_ = nullptr;
	// 影を描いている間のライトの行列。ShadowMapPass が設定して戻す（LightManager が持つ行列。所有しない）
	const Matrix4x4* shadowCullingViewProjection_ = nullptr;
	// 描く物の一覧と、ビューごとの見えている物のリスト
	GameObjectRenderer renderer_;
	// 半透明を距離で並べるときの作業用。毎フレーム確保しないよう使い回す
	struct TransparentEntry
	{
		IRenderable3d* object = nullptr;
		float distanceSquared = 0.0f;
	};
	std::vector<TransparentEntry> transparentEntries_;

	// 動的に作成され、マネージャーが所有するGameObjectのリスト
	std::vector<std::unique_ptr<GameObject>> dynamicGameObjects_;

	// 初期化用にキャッシュされた共有システムポインタ
	Object3dCommon* cachedObject3dCommon_ = nullptr;
	LightManager* cachedLightManager_ = nullptr;
};
} // namespace KCE
