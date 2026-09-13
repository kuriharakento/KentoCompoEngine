#pragma once
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "editor/SelectionContext.h"

struct ImGuiContext;
struct ImGuiSettingsHandler;
struct ImGuiTextBuffer;

namespace KCE
{
/** @brief エディタの初期ドッキング先。LeftBottom は Hierarchy の下（シーンごとのパネル）。 */
enum class EditorDock { Left, Right, RightBottom, Bottom, LeftBottom };
/** @brief EditorDock の数。置き場所ごとの配列の大きさに使う。 */
constexpr size_t kEditorDockCount = 5;

/**
 * @brief エディタの ImGui ウィンドウを置き場所ごとにまとめて描く。
 *
 * - 独立ウィンドウ（Project・道具など）、Settings のページ、Inspector の中身、Scene への重ね描きを登録で受け付ける
 * - 表示のオン・オフや UI の拡大率は imgui.ini の [DebugUI] に覚えておく
 */
class DebugUIManager
{
public:
	static DebugUIManager* GetInstance();
	static bool HasInstance();
#ifdef USE_IMGUI
	/** @brief 初期化する。imgui.ini の読み書き口もここで登録する。 */
	void Initialize();
	/** @brief 登録を全部外して終了する。 */
	void Finalize();
	/**
	 * @brief 独立したエディタウィンドウを登録する。
	 * @param owner 登録元。Unregister で外すときの鍵
	 * @param name ImGui のウィンドウ名。表示のオン・オフもこの名前で覚える
	 * @param draw 中身の描画
	 * @param dock 初めて開くときの置き場所
	 * @param defaultVisible 覚えた表示状態が無いときに開いておくか
	 */
	void RegisterWindow(void* owner, const std::string& name, std::function<void()> draw, EditorDock dock, bool defaultVisible = true);
	/**
	 * @brief Settings のページを登録する。
	 * @param category 左の一覧の分類名（"Rendering" など）
	 * @param name ページ名
	 */
	void RegisterSettingsPage(void* owner, const std::string& category, const std::string& name, std::function<void()> draw);
	/** @brief 選択の種類に対応する Inspector の中身を登録する。種類ごとに1つ。 */
	void RegisterInspector(void* owner, SelectionKind kind, std::function<void(const SelectionItem&)> draw);
	/** @brief Scene 画像の上に描く重ね描き（ギズモなど）を登録する。 */
	void RegisterSceneOverlay(void* owner, std::function<void()> draw);
	/**
	 * @brief Hierarchy の区画を登録する。区画は登録順に折りたたみ見出しで並ぶ。
	 * @param name 見出し（"GameObjects" など）
	 * @param draw 一覧の描画。選んだら SelectionContext へ伝える
	 */
	void RegisterHierarchySection(void* owner, const std::string& name, std::function<void()> draw);
	/** @brief owner が登録した項目をすべて外す。 */
	void Unregister(void* owner);
	/** @brief 登録を全部外す。覚えた表示状態は消さない。 */
	void Clear();
	/** @brief 独立ウィンドウ・Inspector・Settings を描く。毎フレーム Scene ウィンドウの後に呼ぶ。 */
	void Draw();
	/** @brief Scene ウィンドウの中で重ね描きを描く。Scene 画像を描いた直後に呼ぶ。 */
	void DrawSceneOverlays();
	/** @brief 次のフレームで初期レイアウトを組み直すよう頼む。表示状態も初期値に戻す。 */
	void RequestLayoutReset();
	/** @brief 初期レイアウトの組み直しを頼まれているか返す。 */
	bool IsLayoutResetRequested() const { return resetLayoutRequested_; }
	/** @brief 初期レイアウトを組み終えたら呼ぶ。 */
	void ClearLayoutResetRequest() { resetLayoutRequested_ = false; }
	/**
	 * @brief 初期レイアウトでその場所に入れるウィンドウ名を返す。
	 * @return 呼ぶたびに作り直す。次に呼ぶまで有効
	 */
	const std::vector<std::string>& GetDockWindowNames(EditorDock dock);
	/** @brief メニューに独立ウィンドウの表示切り替えを並べる。 */
	void DrawWindowMenu();
	/** @brief UI の拡大率を返す。 */
	float GetUIScale() const { return uiScale_; }
	/** @brief UI の拡大率を変える。文字と余白をまとめて拡大し、imgui.ini に覚える。 */
	void SetUIScale(float scale);
	/** @brief Console を表示するか返す。 */
	bool IsShowConsole() const { return showConsole_; }
	/** @brief Console の表示を変える。変わったときだけ imgui.ini に覚える。 */
	void SetShowConsole(bool show);
#else
	void Initialize() {}
	void Finalize() {}
	void RegisterWindow(void*, const std::string&, std::function<void()>, EditorDock, bool = true) {}
	void RegisterSettingsPage(void*, const std::string&, const std::string&, std::function<void()>) {}
	void RegisterInspector(void*, SelectionKind, std::function<void(const SelectionItem&)>) {}
	void RegisterSceneOverlay(void*, std::function<void()>) {}
	void RegisterHierarchySection(void*, const std::string&, std::function<void()>) {}
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
	/** @brief 独立ウィンドウ1つぶんの登録。 */
	struct Window
	{
		void* owner = nullptr;					//!< 登録元。所有しない
		std::string name;						//!< ImGui のウィンドウ名
		EditorDock dock = EditorDock::Bottom;	//!< 初めて開くときの置き場所
		std::function<void()> draw;				//!< 中身の描画
		bool visible = true;					//!< 今の表示状態
		bool defaultVisible = true;				//!< 初期レイアウトでの表示状態
	};
	/** @brief Settings のページ1つぶんの登録。 */
	struct SettingsPage
	{
		void* owner = nullptr;			//!< 登録元。所有しない
		std::string category;			//!< 左の一覧の分類名
		std::string name;				//!< ページ名
		std::string displayName;		//!< 分類付きの表示名。毎フレーム作らないよう登録時に作る
		std::function<void()> draw;		//!< 中身の描画
	};
	/** @brief Inspector の中身1つぶんの登録。 */
	struct InspectorPage
	{
		void* owner = nullptr;								//!< 登録元。所有しない
		SelectionKind kind = SelectionKind::None;			//!< 受け持つ選択の種類
		std::function<void(const SelectionItem&)> draw;		//!< 中身の描画
	};
	/** @brief Scene への重ね描き1つぶんの登録。 */
	struct Overlay
	{
		void* owner = nullptr;			//!< 登録元。所有しない
		std::function<void()> draw;		//!< 重ね描き
	};
	/** @brief Hierarchy の区画1つぶんの登録。 */
	struct HierarchySection
	{
		void* owner = nullptr;			//!< 登録元。所有しない
		std::string name;				//!< 折りたたみ見出し
		std::function<void()> draw;		//!< 一覧の描画
	};

	/** @brief 1つの Hierarchy に、登録された区画を並べて描く。 */
	void DrawHierarchy();
	/** @brief 1つの Inspector に、今の選択に合った中身を描く。 */
	void DrawInspector();
	/** @brief 左に一覧、右に中身の Settings を描く。 */
	void DrawSettings();
	/**
	 * @brief imgui.ini に設定が無いウィンドウを、同じ置き場所のウィンドウの隣へ入れる。
	 * @details 既に imgui.ini がある環境では初期レイアウトが組まれないので、後から増えたウィンドウが浮かないようにする。
	 */
	void DockOnFirstOpen(const std::string& name, EditorDock dock);
	/** @brief 今の表示状態を覚えて、imgui.ini の書き出しを頼む。 */
	void SaveSettings();
	/** @brief Settings の分類の並びを登録順で作り直す。登録・解除のときだけ呼ぶ。 */
	void RebuildSettingsCategories();
	/** @brief Settings のページが検索欄の文字を含むか返す。大文字と小文字は区別しない。 */
	bool MatchesSettingsFilter(const SettingsPage& page) const;

	// --- imgui.ini の [DebugUI] の読み書き口（ImGuiSettingsHandler に渡す） ---
	static void* ReadSettingsOpen(ImGuiContext* context, ImGuiSettingsHandler* handler, const char* name);
	static void ReadSettingsLine(ImGuiContext* context, ImGuiSettingsHandler* handler, void* entry, const char* line);
	static void ApplySettings(ImGuiContext* context, ImGuiSettingsHandler* handler);
	static void WriteSettings(ImGuiContext* context, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buffer);

	std::vector<Window> windows_;
	std::vector<SettingsPage> settingsPages_;
	// Settings の分類名（登録順）。毎フレーム作らないよう、登録・解除のときだけ作り直す
	std::vector<std::string> settingsCategories_;
	std::vector<InspectorPage> inspectorPages_;
	std::vector<Overlay> overlays_;
	std::vector<HierarchySection> hierarchySections_;
	// GetDockWindowNames の結果。呼ぶたびに作り直すが、領域は使い回す
	std::array<std::vector<std::string>, kEditorDockCount> dockWindowNames_;
	std::string selectedSettingsPage_;
	std::array<char, 128> settingsFilter_{};
	float uiScale_ = 1.0f;
	// ScaleAllSizes は掛け算なので、前回の拡大率との比で掛ける
	float previousUiScale_ = 1.0f;
	bool showConsole_ = true;
	bool resetLayoutRequested_ = false;
#endif
};
} // namespace KCE
