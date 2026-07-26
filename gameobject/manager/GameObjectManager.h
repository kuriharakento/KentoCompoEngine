#pragma once
#include <vector>
#include <string>
#include <memory>

namespace KCE
{
class GameObject;
class CameraManager;
class Camera;
class Object3dCommon;
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
	 * @brief 動的なGameObjectの作成
	 * @param name オブジェクト名
	 * @param tag タグ
	 * @return 生成されたオブジェクトのポインタ（マネージャーが所有）
	 */
	GameObject* CreateGameObject(const std::string& name = "GameObject", const std::string& tag = "GameObject");

	/**
	 * @brief 名前でGameObjectを検索
	 * @param name 検索する名前
	 * @return 最初に見つかったGameObject。無ければnullptr
	 */
	GameObject* Find(const std::string& name) const;

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

public:
	~GameObjectManager() = default;

private:
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

	// 動的に作成され、マネージャーが所有するGameObjectのリスト
	std::vector<std::unique_ptr<GameObject>> dynamicGameObjects_;

	// 初期化用にキャッシュされた共有システムポインタ
	Object3dCommon* cachedObject3dCommon_ = nullptr;
	LightManager* cachedLightManager_ = nullptr;
};
} // namespace KCE
