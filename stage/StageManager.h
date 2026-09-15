#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include "graphics/text/TextMesh3D.h"

namespace KCE
{
class CameraManager;
class GameObject;
class ISubViewProvider;
class LightManager;
class Object3dCommon;
class StageMonitor;
class Text3DRenderer;
struct SelectionItem;

/**
 * @brief ステージに置く物を所有し、編集とファイル保存をまとめる。
 *
 * Text3DRenderer には非所有で登録し、所有物を破棄する前に必ず登録を外す。
 */
class StageManager
{
public:
	static constexpr uint32_t kDefaultMonitorWidth = 480;
	static constexpr uint32_t kDefaultMonitorHeight = 270;
	static constexpr uint32_t kMinMonitorResolution = 16;
	static constexpr uint32_t kMaxMonitorResolution = 4096;
	static constexpr float kDefaultMonitorFramesPerSecond = 24.0f;
	static constexpr float kMinMonitorFramesPerSecond = 1.0f;
	static constexpr float kMaxMonitorFramesPerSecond = 240.0f;

	StageManager() = default;
	~StageManager();
	/**
	 * @brief ステージを初期化して同名ファイルを読む。
	 * @param stageName 拡張子を含まないステージ名
	 * @param text3DRenderer Framework 所有。StageManager より長生きする前提
	 * @param cameraManager Framework 所有。StageManager より長生きする前提
	 * @param subViewProvider サブビューの作成元。Framework 所有
	 * @param object3dCommon 画面オブジェクトの初期化元。Framework 所有
	 * @param lightManager 画面オブジェクトの初期化元。Framework 所有
	 */
	void Initialize(const std::string& stageName, Text3DRenderer* text3DRenderer, CameraManager* cameraManager,
		ISubViewProvider* subViewProvider, Object3dCommon* object3dCommon, LightManager* lightManager);
	/** @brief 所有するモニターを更新する。 */
	void Update();
	/** @brief 現在の内容をステージファイルへ保存する。 */
	bool SaveToFile();
	/** @brief ステージファイルを読む。無ければ空のステージとして成功する。 */
	bool LoadFromFile();
	/** @return 保存後に内容が変わっていれば真。 */
	bool IsDirty() const;
	/** @brief 3D 文字を所有へ移し、Renderer に登録する。 */
	bool AddText3D(const std::string& name, std::unique_ptr<TextMesh3D> mesh);
	/** @brief 3D 文字を登録解除し、所有権を返す。 */
	std::unique_ptr<TextMesh3D> RemoveText3D(const std::string& name);
	/** @return このステージが所有する名前なら真。 */
	bool OwnsText3D(const std::string& name) const;
	/** @return ステージ所有の3D文字。見つからなければ nullptr。 */
	TextMesh3D* GetOwnedText3D(const std::string& name);

	/** @brief モニターの保存・編集対象。 */
	struct MonitorState
	{
		Vector3 screenPosition{};
		Vector3 screenRotation{};
		Vector3 screenScale{ 1.0f, 1.0f, 1.0f };
		Vector3 cameraPosition{};
		Vector3 cameraRotation{};
		uint32_t width = kDefaultMonitorWidth;
		uint32_t height = kDefaultMonitorHeight;
		float framesPerSecond = kDefaultMonitorFramesPerSecond;
	};

	/** @brief StageManager が所有するモニター一式。 */
	struct MonitorEntry
	{
		std::string name;
		std::string cameraName;
		MonitorState state;
		std::unique_ptr<GameObject> screen;
		std::unique_ptr<StageMonitor> monitor;
	};

	/**
	 * @brief モニター一式を登録して動作を開始する。
	 * @param entry 成功したときだけ所有権を受け取る（中身は空になる）。失敗したら呼び出し側に残る
	 * @return 登録できたら真
	 */
	bool AddStageMonitor(std::unique_ptr<MonitorEntry>& entry);
	/** @brief モニターを停止・登録解除し、所有権を返す。 */
	std::unique_ptr<MonitorEntry> RemoveStageMonitor(const std::string& name);
	/** @brief 保存済みの状態をモニターへ適用する。 */
	bool ApplyMonitorState(const std::string& name, const MonitorState& state, bool recreateView);
	const std::string& GetStageName() const { return stageName_; }
	uint64_t GetLifetimeId() const { return lifetimeId_; }
	/** @brief Undo コマンドがシーン破棄後の Manager を触らないための確認。 */
	static bool IsAlive(const StageManager* manager, uint64_t lifetimeId);

private:
	struct TextEntry { std::string name; std::unique_ptr<TextMesh3D> mesh; };
	MonitorEntry* FindMonitor(const std::string& name);
	const MonitorEntry* FindMonitor(const std::string& name) const;
	std::unique_ptr<MonitorEntry> CreateMonitor(const std::string& name, const std::string& cameraName, const MonitorState& state);
	nlohmann::ordered_json Serialize() const;
	bool Deserialize(const nlohmann::json& json, std::string& outError);
	std::filesystem::path GetFilePath() const;
	void Clear();
	/** @brief 今の中身を「保存した時点」として覚える。保存と読み込みのあとに呼ぶ */
	void CaptureSavedState();
#ifdef USE_IMGUI
	void RegisterDebugUI();
	void DrawHierarchyImGui();
	void DrawInspectorImGui(const SelectionItem& item);
#endif
	/** @brief 保存（か読み込み）した時点の3D文字。未保存かを JSON にせず比べるために持つ */
	struct SavedText { std::string name; std::string text; TextMesh3D::Params params; };
	struct SavedMonitor { std::string name; std::string cameraName; MonitorState state; };
	std::string stageName_;
	std::vector<TextEntry> texts3D_;
	std::vector<std::unique_ptr<MonitorEntry>> monitors_;
	// 保存（か読み込み）した時点の中身。IsDirty() が毎フレームこれと比べる
	std::vector<SavedText> savedTexts_;
	std::vector<SavedMonitor> savedMonitors_;
	uint64_t lifetimeId_ = 0;
	// Framework 所有。StageManager より長生きする前提
	Text3DRenderer* text3DRenderer_ = nullptr;
	// Framework 所有。StageManager より長生きする前提
	CameraManager* cameraManager_ = nullptr;
	// Framework 所有。StageManager より長生きする前提
	ISubViewProvider* subViewProvider_ = nullptr;
	// Framework 所有。StageManager より長生きする前提
	Object3dCommon* object3dCommon_ = nullptr;
	// Framework 所有。StageManager より長生きする前提
	LightManager* lightManager_ = nullptr;
#ifdef USE_IMGUI
	std::array<char, 128> newName_{};
	std::array<char, 512> newText_{};
	std::array<char, 128> newMonitorName_{};
	bool createPopupRequested_ = false;
	bool createMonitorPopupRequested_ = false;
	std::string createError_;
	std::string createMonitorError_;
	MonitorState editStartState_{};
	std::string editingMonitorName_;
	bool editMonitorCameraWithGizmo_ = false;
#endif
};
} // namespace KCE
