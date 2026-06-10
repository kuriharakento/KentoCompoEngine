#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

#ifdef USE_IMGUI
/**
 * @brief デバッグUIの表示エリア
 * @details 各エリアの使い分けの目安：
 * - Hierarchy: オブジェクトの階層構造、構成リスト、選択・切り替え用（例: SceneManager, ECSエンティティ）
 * - Inspector: 詳細パラメータの調整、コンポーネント変数などのリアルタイム変更用（例: 各種スライダー, マネージャー設定）
 * - Console: テキスト出力、大量のログやリアルタイムの衝突情報などの表示用（例: ConsoleLog, コライダーリスト）
 * - Scene: ゲーム画面へのオーバーレイ表示用（例: ミニマップ, HUDのテスト表示）
 * - Project: リソース管理、画面の横幅を広く使いたい横広エディタ用（例: JSON Editorテーブル, アセットブラウザ）
 */
enum class DebugUIArea
{
	Hierarchy, // 左側（構成・選択）
	Inspector, // 右側（パラメータ詳細・調整）
	Console,   // 下部（テキストログ・出力情報）
	Scene,     // 中央（メイン画面重ね合わせ）
	Project    // 下部（横広エディタ・アセット管理）
};

/**
 * @brief デバッグUI表示情報を管理する構造体
 */
struct DebugUI
{
	std::string name;
	std::function<void()> drawFunc;
	DebugUIArea area;
	bool visible = true; // Toolsメニューからの表示切替
};
#else
enum class DebugUIArea
{
	Hierarchy,
	Inspector,
	Console,
	Scene
};
#endif

/**
 * @brief デバッグUI管理クラス
 * @details 各種デバッグ表示の登録・解除および一括描画、初期レイアウトの要求と管理を行うシングルトン
 */
class DebugUIManager
{
public:
	/**
	 * @brief シングルトンインスタンスを取得
	 * @return インスタンスへのポインタ
	 */
	static DebugUIManager* GetInstance();

	/**
	 * @brief インスタンスが有効かどうかを取得
	 * @return インスタンスが存在すれば真
	 */
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
	 * @brief デバッグUIを登録する
	 * @param owner 登録するオブジェクトのポインタ（登録キー）
	 * @param name デバッグウィンドウ名（識別子）
	 * @param drawFunc 描画コールバック。通常は [this]() { this->DrawImGui(); } の形式
	 * @param area 初期表示エリア（用途に応じて選択。詳細は DebugUIArea の解説を参照）
	 */
	void RegisterDebugUI(void* owner, const std::string& name, std::function<void()> drawFunc, DebugUIArea area = DebugUIArea::Inspector);

	/**
	 * @brief デバッグUIの登録を解除する
	 * @param owner 登録元のオブジェクトのポインタ
	 */
	void UnregisterDebugUI(void* owner);

	/**
	 * @brief すべての登録済みUIをクリアする（シーン切替時に使用）
	 */
	void Clear();

	/**
	 * @brief 登録されたすべてのデバッグウィンドウを描画する
	 */
	void Draw();

	/**
	 * @brief 初期レイアウトの再構築を要求する
	 */
	void RequestLayoutReset();

	/**
	 * @brief 初期レイアウトのリセットが要求されているかを取得
	 * @return 要求されている場合は真
	 */
	bool IsLayoutResetRequested() const;

	/**
	 * @brief レイアウトリセット要求フラグをクリアする
	 */
	void ClearLayoutResetRequest();

	/**
	 * @brief 指定されたエリアの（visible=true な）デバッグUIを描画する
	 */
	void DrawArea(DebugUIArea area);

	/**
	 * @brief Toolsメニュー用のサブメニューを描画する（カテゴリ別グループ化）
	 * @details ImGui::BeginMenu("Tools") の中から呼ぶこと
	 */
	void DrawToolsMenu();

	/**
	 * @brief 特定のデバッグUIのエリアを変更する
	 */
	void SetDebugUIArea(void* owner, DebugUIArea area);
	void SetDebugUIArea(const std::string& name, DebugUIArea area);

	// === レイアウトパネルの表示・非表示フラグ ===
	bool IsShowHierarchy() const { return showHierarchy_; }
	void SetShowHierarchy(bool show) { showHierarchy_ = show; }

	bool IsShowInspector() const { return showInspector_; }
	void SetShowInspector(bool show) { showInspector_ = show; }

	bool IsShowConsole() const { return showConsole_; }
	void SetShowConsole(bool show) { showConsole_ = show; }

	bool IsShowProject() const { return showProject_; }
	void SetShowProject(bool show) { showProject_ = show; }

	/**
	 * @brief UIスケール値を取得する
	 * @return スケール値
	 */
	float GetUIScale() const;

	/**
	 * @brief UIスケール値を設定し、適用する
	 * @param scale スケール値
	 */
	void SetUIScale(float scale);

	/**
	 * @brief UIスケール値を実際にImGuiへ適用する
	 * @param scale スケール値
	 */
	void ApplyUIScale(float scale);

#ifdef USE_IMGUI
	struct SavedUIState
	{
		DebugUIArea area;
		bool visible;
	};
#endif

private:
	// 登録されたコールバック情報を保持するマップ（owner -> UIリスト）
#ifdef USE_IMGUI
	std::unordered_map<void*, std::vector<DebugUI>> debugUIs_;

	void SaveLayout();
	void ClearLoadedStates();
	SavedUIState& GetOrAddLoadedState(const std::string& name);
	void WriteAllSettings(struct ImGuiTextBuffer* buf);
	void ApplyLoadedStatesToActiveUIs();
#endif

	// UIスケール
	float uiScale_ = 1.0f;

	// レイアウトパネルの開閉フラグ
	bool showHierarchy_ = true;
	bool showInspector_ = true;
	bool showConsole_   = true;
	bool showProject_   = true;

	// レイアウトリセット要求フラグ
	bool resetLayoutRequested_ = false;

public:
	~DebugUIManager() = default;

private:
	static std::unique_ptr<DebugUIManager> instance_;

	DebugUIManager() = default;
	DebugUIManager(const DebugUIManager&) = delete;
	DebugUIManager& operator=(const DebugUIManager&) = delete;
};
