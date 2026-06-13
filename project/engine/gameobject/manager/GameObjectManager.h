#pragma once
#include <vector>
#include <string>
#include <memory>

class GameObject;
class CameraManager;

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
	 */
	void DrawShadow();

	/**
	 * @brief G-Buffer描画
	 */
	void DrawGBuffer();

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

public:
	~GameObjectManager() = default;

private:
	GameObjectManager() = default;
	GameObjectManager(const GameObjectManager&) = delete;
	GameObjectManager& operator=(const GameObjectManager&) = delete;

private:
	static std::unique_ptr<GameObjectManager> instance_;

	// 管理しているGameObjectのリスト（非所有ポインタ）
	std::vector<GameObject*> gameObjects_;
};
