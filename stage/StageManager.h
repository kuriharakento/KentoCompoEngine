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
	StageManager() = default;
	~StageManager();
	/**
	 * @brief ステージを初期化して同名ファイルを読む。
	 * @param stageName 拡張子を含まないステージ名
	 * @param text3DRenderer Framework 所有。StageManager より長生きする前提
	 * @param cameraManager Framework 所有。StageManager より長生きする前提
	 */
	void Initialize(const std::string& stageName, Text3DRenderer* text3DRenderer, CameraManager* cameraManager);
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
	const std::string& GetStageName() const { return stageName_; }
	uint64_t GetLifetimeId() const { return lifetimeId_; }
	/** @brief Undo コマンドがシーン破棄後の Manager を触らないための確認。 */
	static bool IsAlive(const StageManager* manager, uint64_t lifetimeId);

private:
	struct TextEntry { std::string name; std::unique_ptr<TextMesh3D> mesh; };
	nlohmann::ordered_json Serialize() const;
	bool Deserialize(const nlohmann::json& json, std::string& outError);
	std::filesystem::path GetFilePath() const;
	void Clear();
#ifdef USE_IMGUI
	void RegisterDebugUI();
	void DrawHierarchyImGui();
	void DrawInspectorImGui(const SelectionItem& item);
#endif
	std::string stageName_;
	std::vector<TextEntry> texts3D_;
	std::string savedState_;
	uint64_t lifetimeId_ = 0;
	// Framework 所有。StageManager より長生きする前提
	Text3DRenderer* text3DRenderer_ = nullptr;
	// Framework 所有。StageManager より長生きする前提
	CameraManager* cameraManager_ = nullptr;
#ifdef USE_IMGUI
	std::array<char, 128> newName_{};
	std::array<char, 512> newText_{};
	bool createPopupRequested_ = false;
	std::string createError_;
#endif
};
} // namespace KCE
