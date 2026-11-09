#pragma once

#include <memory>
#include <string>

#include "math/Vector3.h"
#include "level/ILevelElement.h"

namespace KCE
{
class CameraManager;
class GameObject;
class RenderView;
class StageMonitor;

/** @brief ステージモニターの画面の縦横比。固定にすると、幅を基準に高さが決まる */
enum class MonitorScreenAspect
{
	Aspect16x9,
	Aspect4x3,
	Aspect1x1,
	Aspect9x16,
	Aspect21x9,
	Free,
};

/**
 * @brief ステージに置くモニター 1 つ。画面の板（GameObject）と、映像を貼る StageMonitor を所有する。
 *
 * JSON は旧ステージ（version 2）の "monitors" と同じ形:
 *   { "name": "screen", "screen": { "position": [..], "rotation": [..], "scale": [..] },
 *     "camera": "cam", "aspect": "16:9" }
 *
 * - 映すカメラは、同じステージの CameraElement を名前で探す。カメラが無い・消えたときは、何も映さない
 * - 画面が本編カメラに見えているときだけ、映すカメラを描かせる（カメラは次のフレームに反映する）
 * - version 1 の旧形式（カメラをモニター内に持つ）は読めない。version 2 以降の保存で使うこと
 */
class MonitorElement final : public ILevelElement
{
public:
	~MonitorElement() override;

	/** @brief LevelManager に登録する、"monitors" の種類。カメラの種類より後に登録すること */
	static LevelElementType MakeType();

	const char* GetTypeKey() const override { return "monitors"; }
	void Update(float deltaTime) override;
	bool OnRename(const std::string& newName) override;
	void OnLevelBound() override;
	void OnElementRenamed(const std::string& oldName, const std::string& newName) override;
	nlohmann::ordered_json Serialize() const override;

#ifdef USE_IMGUI
	void DrawHierarchyRow() override;
	void DrawInspector(const SelectionItem& selected) override;
	bool GetGizmoPose(const SelectionItem& selected, Matrix4x4& world, uint32_t& operations) const override;
	void ApplyGizmo(const SelectionItem& selected, const GizmoResult& result, uint32_t dragId) override;
#endif

private:
	friend struct MonitorElementFactory;
	MonitorElement(std::string name, std::string cameraName, MonitorScreenAspect aspect, GameObject* screen, CameraManager* cameraManager);
	/** @brief 画面の縦横比とカメラの縦横比から、映像を切り抜く範囲を決める */
	void UpdateCrop();

	// 所有は GameObjectManager。画面の板
	GameObject* screen_ = nullptr;
	std::unique_ptr<StageMonitor> monitor_;
	std::string cameraName_;
	MonitorScreenAspect aspect_ = MonitorScreenAspect::Free;
	// Framework 所有
	CameraManager* cameraManager_ = nullptr;
	// 今つないでいる映像と、そのときの切り抜きの元。変わったときだけ切り抜き直す
	RenderView* attachedView_ = nullptr;
	float cropScreenAspect_ = 0.0f;
	float cropCameraAspect_ = 0.0f;
};
} // namespace KCE
