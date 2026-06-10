#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

#ifdef USE_IMGUI
/**
 * @brief デバッグUIの表示エリアを表す列挙型
 */
enum class DebugUIArea
{
	Hierarchy, // 左側（Hierarchyなどと同じエリア）
	Inspector, // 右側（Inspectorなどと同じエリア）
	Console,   // 下部（Consoleなどと同じエリア）
	Scene,     // 中央（Sceneなどと同じメインエリア）
	Project    // 下部（Projectなどと同じエリア）
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
	 * @param area 初期表示エリア（未指定時は Inspector）
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
