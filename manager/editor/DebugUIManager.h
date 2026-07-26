#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct ImGuiTextBuffer;

namespace KCE
{
/**
 * @brief デバッグUIの表示エリア
 * @details 各エリアの使い分け：
 * - Hierarchy: 構成リスト、選択用（例: SceneManager）
 * - Inspector: パラメータ詳細・調整用
 * - Console: テキストログ出力用
 * - Scene: ゲーム画面へのオーバーレイ用
 * - Project: 横広エディタ・アセット管理用
 */
#ifdef USE_IMGUI
enum class DebugUIArea
{
	Hierarchy,
	Inspector,
	Console,
	Scene,
	Project
};

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

class DebugUIManager
{
public:
	static DebugUIManager* GetInstance();
	static bool HasInstance();

#ifdef USE_IMGUI
	void Initialize();
	void Finalize();

	/**
	 * @brief デバッグUIを登録する
	 * @param owner 登録元のオブジェクトキー
	 * @param name ウィンドウ名
	 * @param drawFunc 描画コールバック
	 * @param area 表示エリア
	 */
	void RegisterDebugUI(void* owner, const std::string& name, std::function<void()> drawFunc, DebugUIArea area = DebugUIArea::Inspector);
	void UnregisterDebugUI(void* owner);
	void Clear();
	void Draw();

	void RequestLayoutReset();
	bool IsLayoutResetRequested() const;
	void ClearLayoutResetRequest();

	void DrawArea(DebugUIArea area);

	/**
	 * @brief Toolsメニュー用のサブメニューを描画する
	 * @details ImGui::BeginMenu("Tools") の中から呼ぶこと
	 */
	void DrawToolsMenu();

	void SetDebugUIArea(void* owner, DebugUIArea area);
	void SetDebugUIArea(const std::string& name, DebugUIArea area);

	bool IsShowHierarchy() const { return showHierarchy_; }
	void SetShowHierarchy(bool show) { showHierarchy_ = show; }

	bool IsShowInspector() const { return showInspector_; }
	void SetShowInspector(bool show) { showInspector_ = show; }

	bool IsShowConsole() const { return showConsole_; }
	void SetShowConsole(bool show) { showConsole_ = show; }

	bool IsShowProject() const { return showProject_; }
	void SetShowProject(bool show) { showProject_ = show; }

	float GetUIScale() const;
	void SetUIScale(float scale);
	void ApplyUIScale(float scale);

	struct SavedUIState
	{
		DebugUIArea area;
		bool visible;
	};

#else
	// --- 非ImGui環境用の空実装（元のドキュメントや引数名を残して可読性を維持） ---
	void Initialize() {}
	void Finalize() {}

	/**
	 * @brief デバッグUIを登録する（非ImGui時は何もしない）
	 */
	void RegisterDebugUI(void* owner, const std::string& name, std::function<void()> drawFunc, DebugUIArea area = DebugUIArea::Inspector) {}
	void UnregisterDebugUI(void* owner) {}
	void Clear() {}
	void Draw() {}

	void RequestLayoutReset() {}
	bool IsLayoutResetRequested() const { return false; }
	void ClearLayoutResetRequest() {}

	void DrawArea(DebugUIArea area) {}

	/**
	 * @brief Toolsメニュー用のサブメニューを描画する（非ImGui時は何もしない）
	 */
	void DrawToolsMenu() {}

	void SetDebugUIArea(void* owner, DebugUIArea area) {}
	void SetDebugUIArea(const std::string& name, DebugUIArea area) {}

	bool IsShowHierarchy() const { return false; }
	void SetShowHierarchy(bool show) {}

	bool IsShowInspector() const { return false; }
	void SetShowInspector(bool show) {}

	bool IsShowConsole() const { return false; }
	void SetShowConsole(bool show) {}

	bool IsShowProject() const { return false; }
	void SetShowProject(bool show) {}

	float GetUIScale() const { return 1.0f; }
	void SetUIScale(float scale) {}
	void ApplyUIScale(float scale) {}
#endif

public:
	~DebugUIManager() = default;

private:
	static std::unique_ptr<DebugUIManager> instance_;

	DebugUIManager() = default;
	DebugUIManager(const DebugUIManager&) = delete;
	DebugUIManager& operator=(const DebugUIManager&) = delete;

#ifdef USE_IMGUI
	std::unordered_map<void*, std::vector<DebugUI>> debugUIs_;

	void SaveLayout();
	void ClearLoadedStates();
	SavedUIState& GetOrAddLoadedState(const std::string& name);
	void WriteAllSettings(::ImGuiTextBuffer* buf);
	void ApplyLoadedStatesToActiveUIs();

	float uiScale_ = 1.0f;

	bool showHierarchy_ = true;
	bool showInspector_ = true;
	bool showConsole_ = true;
	bool showProject_ = true;

	bool resetLayoutRequested_ = false;
#endif
};
} // namespace KCE
