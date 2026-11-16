#pragma once
#include "core/Guid.h"
#include <functional>
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
	 * @brief GameObject一覧を描画する
	 */
	void DrawListImGui();

	/**
	 * @brief 選択中GameObjectの詳細を描画する
	 */
	void DrawInspectorImGui();

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

	/** @brief 行の操作を、描画中に一覧を変えないよう、描き終えてから行うための受け皿。 */
	struct PendingActions
	{
		GameObject* toDelete = nullptr;
		GameObject* toDuplicate = nullptr;
		// 作成など、一覧を変える操作。描き終えてから順に行う
		std::vector<std::function<void()>> deferred;
	};

	/**
	 * @brief 作り方を選ぶメニューの中身（新規 / 既存のプレハブから / 既存のJSONから）を描く。
	 * @param parent 子として作るときの親。nullptr なら根として作る
	 */
	void DrawCreateMenu(GameObject* parent, PendingActions& actions);

	/** @brief 親の中でぶつからない子の名前を返す。 */
	static std::string MakeUniqueChildName(const GameObject* parent, const std::string& baseName);

	/** @brief 自分から親をたどった、一番上の GameObject。保存はここから行う。 */
	static GameObject* GetRoot(GameObject* object);

	/** @brief プレハブから作って選ぶ。parent が無ければ根として作る。 */
	GameObject* CreateFromPrefabFile(const std::string& file, GameObject* parent);

	/** @brief Resources/json/gameobject の JSON から作って選ぶ。parent が無ければ根として作る。 */
	GameObject* CreateFromJsonFile(const std::string& file, GameObject* parent);

	/** @brief 1つの GameObject の行を描き、子があれば入れ子で描く。 */
	void DrawObjectNode(GameObject* object, PendingActions& actions);

	/**
	 * @brief GameObject を1つ新規に作って選ぶ。
	 * @param parent 子として作るときの親。nullptr なら根として作る
	 * @param name 付ける名前。空なら既定の名前
	 */
	GameObject* CreateAndSelectNew(GameObject* parent = nullptr, const std::string& name = std::string());

	/** @brief 新規作成の名前入力ダイアログと、保存結果の通知ダイアログを描く。 */
	void DrawDialogs(bool drawCreate, bool drawNotice);

	/** @brief 通知ダイアログを出すよう頼む（次に DrawDialogs が描くとき開く）。 */
	void ShowNotice(const std::string& title, const std::string& message);

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
	/** @brief Resources/json/prefab のJSONファイル一覧を更新する。 */
	void UpdatePrefabFileList();

private:
	static std::unique_ptr<GameObjectEditor> instance_;

	// 現在選択されているGameObject
	GameObject* selected_ = nullptr;

	// セーブ・ロード用のファイル名入力バッファ
	char fileNameBuf_[128] = "gameObject.json";
	char prefabFileNameBuf_[128] = "prefab.json";
	// 一覧ウィンドウの検索文字列
	char filter_[128] = "";

	// 新規作成の名前入力ダイアログ
	bool openCreateDialog_ = false;
	Guid createParentGuid_;        // 子として作るときの親。無効なら根
	char newNameBuf_[128] = "";

	// 通知ダイアログ（保存の結果など）
	bool openNotice_ = false;
	std::string noticeTitle_;
	std::string noticeMessage_;

	// 追加可能なコンポーネントタイプのリスト
	std::vector<std::string> availableComponents_;
	int selectedCompIndex_ = 0;

	// 既存のJSONファイルリスト
	std::vector<std::string> jsonFiles_;
	int selectedJsonIndex_ = 0;
	std::vector<std::string> prefabFiles_;
	int selectedPrefabIndex_ = 0;
};
} // namespace KCE
