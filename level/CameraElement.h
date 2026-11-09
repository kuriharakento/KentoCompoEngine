#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "math/Vector3.h"
#include "level/ILevelElement.h"

namespace KCE
{
class CameraManager;
class ISubViewProvider;
class RenderView;

/**
 * @brief ステージに置くカメラ 1 つ。CameraManager のカメラと、その映像を描くサブビューを所有する。
 *
 * JSON は旧ステージの "cameras" と同じ形:
 *   { "name": "cam", "position": [x,y,z], "rotation": [x,y,z], "resolution": [w,h], "framesPerSecond": 24 }
 *
 * - サブビューは、モニターが映すときだけ描けば足りるので、作った直後は止めておく（SetViewEnabled で切り替える）
 * - 位置と向きは CameraManager のカメラが持つ。保存のときはそこから読む（ギズモや Inspector で動かした結果が入る）
 */
class CameraElement final : public ILevelElement
{
public:
	static constexpr uint32_t kDefaultWidth = 480;
	static constexpr uint32_t kDefaultHeight = 270;
	static constexpr uint32_t kMinResolution = 16;
	static constexpr uint32_t kMaxResolution = 4096;
	static constexpr float kDefaultFramesPerSecond = 24.0f;
	static constexpr float kMinFramesPerSecond = 1.0f;
	static constexpr float kMaxFramesPerSecond = 240.0f;

	/** @brief 保存・編集する値 */
	struct State
	{
		Vector3 position{};
		Vector3 rotation{};
		uint32_t width = kDefaultWidth;
		uint32_t height = kDefaultHeight;
		float framesPerSecond = kDefaultFramesPerSecond;
	};

	~CameraElement() override;

	/** @brief LevelManager に登録する、"cameras" の種類 */
	static LevelElementType MakeType();
	/**
	 * @brief カメラとサブビューを作って要素にする
	 * @return 作れなければ nullptr（名前の重複、ビューを作れないなど）
	 */
	static std::unique_ptr<CameraElement> Create(const std::string& name, const State& state, CameraManager* cameraManager, ISubViewProvider* provider);

	const char* GetTypeKey() const override { return "cameras"; }
	nlohmann::ordered_json Serialize() const override;
	bool OnRename(const std::string& newName) override;

	uint32_t GetWidth() const { return state_.width; }
	uint32_t GetHeight() const { return state_.height; }
	/** @brief 映像が見られていると伝える。次のフレームの Update で、映像を描かせる */
	void RequestView() { viewRequested_ = true; }
	void Update(float deltaTime) override;

	/** @return モニターが映す映像。解像度を変えると作り直されるので、持ち続けず毎回引く。所有は Framework */
	RenderView* GetView() const { return view_; }
	/** @brief 映像を描くか。モニターが見えているときだけ描けば足りる */
	void SetViewEnabled(bool enabled);

#ifdef USE_IMGUI
	void DrawHierarchyRow() override;
	void DrawInspector(const SelectionItem& selected) override;
#endif

private:
	CameraElement(std::string name, const State& state, CameraManager* cameraManager, ISubViewProvider* provider);
	/** @brief カメラとサブビューを作る */
	bool Initialize();
	/** @brief state_ の解像度と更新間隔を、カメラとビューへ反映する。ビューの解像度と違うときは、ビューを作り直す */
	bool Apply();

	// 編集中の値。解像度は、Inspector で離したときにビューへ反映する
	State state_;
	// 今のビューの解像度
	uint32_t viewWidth_ = 0;
	uint32_t viewHeight_ = 0;
	// Framework 所有。CameraElement より長生きする
	CameraManager* cameraManager_ = nullptr;
	ISubViewProvider* provider_ = nullptr;
	// 所有は provider。DestroySubView まで有効
	RenderView* view_ = nullptr;
	// モニターから見られているか。Update が1回使うと戻す
	bool viewRequested_ = false;
};
} // namespace KCE
