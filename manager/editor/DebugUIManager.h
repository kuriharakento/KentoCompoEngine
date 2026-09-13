#pragma once
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "editor/SelectionContext.h"

namespace KCE
{
/** @brief エディタの初期ドッキング先。 */
enum class EditorDock { Left, Right, RightBottom, Bottom };

class DebugUIManager
{
public:
	static DebugUIManager* GetInstance();
	static bool HasInstance();
#ifdef USE_IMGUI
	void Initialize();
	void Finalize();
	/** @brief 独立したエディタウィンドウを登録する。 */
	void RegisterWindow(void* owner, const std::string& name, EditorDock dock, std::function<void()> draw, bool defaultVisible = true);
	void RegisterWindow(void* owner, const std::string& name, std::function<void()> draw, EditorDock dock, bool defaultVisible = true);
	/** @brief 既存の登録順で独立ウィンドウを登録する。 */
	/** @brief Settings のページを登録する。 */
	void RegisterSettingsPage(void* owner, const std::string& category, const std::string& name, std::function<void()> draw);
	/** @brief 選択種別に対応する Inspector の内容を登録する。 */
	void RegisterInspector(void* owner, SelectionKind kind, std::function<void(const SelectionItem&)> draw);
	/** @brief Scene 画像の上に描くオーバーレイを登録する。 */
	void RegisterSceneOverlay(void* owner, std::function<void()> draw);
	/** @brief owner が登録した項目をすべて外す。 */
	void Unregister(void* owner);
	void Clear();
	void Draw();
	void DrawSceneOverlays();
	void RequestLayoutReset();
	bool IsLayoutResetRequested() const { return resetLayoutRequested_; }
	void ClearLayoutResetRequest() { resetLayoutRequested_ = false; }
	const std::vector<std::string>& GetDockWindowNames(EditorDock dock);
	void DrawWindowMenu();
	float GetUIScale() const { return uiScale_; }
	void SetUIScale(float scale);
	bool IsShowConsole() const { return showConsole_; }
	void SetShowConsole(bool show) { showConsole_ = show; }
#else
	void Initialize() {}
	void Finalize() {}
	void RegisterWindow(void*, const std::string&, EditorDock, std::function<void()>, bool = true) {}
	void RegisterSettingsPage(void*, const std::string&, const std::string&, std::function<void()>) {}
	void RegisterInspector(void*, SelectionKind, std::function<void(const SelectionItem&)>) {}
	void RegisterSceneOverlay(void*, std::function<void()>) {}
	void Unregister(void*) {}
	void Clear() {}
	void Draw() {}
	void DrawSceneOverlays() {}
	void RequestLayoutReset() {}
	bool IsLayoutResetRequested() const { return false; }
	void ClearLayoutResetRequest() {}
	const std::vector<std::string>& GetDockWindowNames(EditorDock) { static const std::vector<std::string> empty; return empty; }
	void DrawWindowMenu() {}
	float GetUIScale() const { return 1.0f; }
	void SetUIScale(float) {}
	bool IsShowConsole() const { return false; }
	void SetShowConsole(bool) {}
#endif
	~DebugUIManager() = default;
private:
	static std::unique_ptr<DebugUIManager> instance_;
	friend std::unique_ptr<DebugUIManager> std::make_unique<DebugUIManager>();
	DebugUIManager() = default;
#ifdef USE_IMGUI
	struct Window
	{
		void* owner = nullptr; //!< 登録元。所有しない。
		std::string name; //!< ImGui ウィンドウ名。
		EditorDock dock = EditorDock::Bottom; //!< 初期ドッキング先。
		std::function<void()> draw; //!< 中身の描画。
		bool visible = true; //!< 表示状態。
		bool defaultVisible = true; //!< 初期表示状態。
	};
	struct SettingsPage
	{
		void* owner = nullptr; //!< 登録元。所有しない。
		std::string category; //!< 左側の分類名。
		std::string name; //!< ページ名。
		std::string displayName; //!< 分類付きの表示名。
		std::function<void()> draw; //!< 中身の描画。
	};
	struct InspectorPage { void* owner = nullptr; SelectionKind kind = SelectionKind::None; std::function<void(const SelectionItem&)> draw; };
	struct Overlay { void* owner = nullptr; std::function<void()> draw; };
	void DrawInspector();
	void DrawSettings();
	void SaveSettings();
	std::vector<Window> windows_;
	std::vector<SettingsPage> settingsPages_;
	std::vector<InspectorPage> inspectorPages_;
	std::vector<Overlay> overlays_;
	std::array<std::vector<std::string>, 4> dockWindowNames_;
	std::unordered_map<std::string, bool> savedVisibility_;
	std::string selectedSettingsPage_;
	std::array<char, 128> settingsFilter_{};
	float uiScale_ = 1.0f;
	float previousUiScale_ = 1.0f;
	bool showConsole_ = true;
	bool resetLayoutRequested_ = false;
#endif
};
} // namespace KCE
