#pragma once
#include <memory>
#include <string>
#include <vector>

namespace KCE
{
class GameObject;

/**
 * @brief ゲームオブジェクトエディタクラス
 * @details ImGuiを利用してGameObjectの階層、コンポーネント追加・削除、
 *          プロパティ編集、Jsonセーブ・ロードを支援するシングルトンデバッグUI
 */
class GameObjectEditor
{
public:
	static GameObjectEditor* GetInstance();
	static bool HasInstance();

	/**
	 * @brief 初期化処理
	 */
	void Initialize();

	/**
	 * @brief 終了処理
	 */
	void Finalize();

	/**
	 * @brief ImGuiによる編集UIを描画する
	 */
	void DrawImGui();

	/**
	 * @brief GameObjectが削除された際に呼び出し、ポインタの安全性を担保する
	 */
	void OnGameObjectRemoved(GameObject* gameObject);

	~GameObjectEditor() = default;

private:
	friend std::unique_ptr<GameObjectEditor> std::make_unique<GameObjectEditor>();
	GameObjectEditor() = default;
	GameObjectEditor(const GameObjectEditor&) = delete;
	GameObjectEditor& operator=(const GameObjectEditor&) = delete;

	/**
	 * @brief コンポーネントを新規作成してGameObjectに追加する
	 * @param owner 対象のGameObject
	 * @param compTypeName コンポーネントの型名
	 */
	void AddComponentByName(GameObject* owner, const std::string& compTypeName);

	/**
	 * @brief Resources/json ディレクトリから既存のJSONファイル一覧を取得する
	 */
	void UpdateJsonFileList();

private:
	static std::unique_ptr<GameObjectEditor> instance_;

	// 現在選択されているGameObject
	GameObject* selected_ = nullptr;

	// セーブ・ロード用のファイル名入力バッファ
	char fileNameBuf_[128] = "gameObject.json";

	// 追加可能なコンポーネントタイプのリスト
	std::vector<std::string> availableComponents_;
	int selectedCompIndex_ = 0;

	// 既存のJSONファイルリスト
	std::vector<std::string> jsonFiles_;
	int selectedJsonIndex_ = 0;
};
} // namespace KCE
