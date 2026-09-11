#pragma once
#include <cstdint>
#include <memory>
#include <string>

#include "graphics/postfx/FullscreenPass.h"
#include "math/MatrixFunc.h"
#include "math/Vector2.h"
#include "math/Vector3.h"

namespace KCE
{
class Camera;
class DirectXCommon;
class FrameConstantAllocator;
class RenderTexture;
class RenderView;
class SrvManager;

/**
 * @brief 被写界深度（ピントの合っていない所をぼかす）
 *
 * @details HDR のシーンカラーと深度から、カメラからの距離でぼけの大きさを決めて
 *          円形に集めてぼかす。結果は一度自分のターゲットへ描いてから、シーンカラーへ書き戻す。
 *          ブルームやトーンマップより前に掛けるので、明るい点は大きな玉ぼけになる。
 *          シーンカラーが読める状態（解決の後）で呼ぶこと。
 */
class DepthOfFieldRenderer
{
public:
	struct Settings
	{
		bool enabled = false;
		// ピントを合わせる距離（カメラから）
		float focusDistance = 10.0f;
		// ピントが合って見える奥行きの幅
		float focusRange = 4.0f;
		// 一番ぼけたときの半径（ピクセル）
		float maxBlurPixels = 8.0f;
	};

	/** @brief DepthOfField.PS.hlsl の cbuffer DofConstants と一致させること */
	struct ConstantsForGPU
	{
		Matrix4x4 invViewProjection;
		Vector3 cameraPosition;
		float focusDistance;
		float focusRange;
		float maxBlurPixels;
		Vector2 invTextureSize;
	};
	static_assert(sizeof(ConstantsForGPU) == 96, "DofConstants のサイズがシェーダー側と一致しません");

	~DepthOfFieldRenderer();

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height);
	void Resize(uint32_t width, uint32_t height);

	/**
	 * @brief ぼかしてシーンカラーへ書き戻す
	 * @param camera このビューのカメラ
	 * @param view 対象のビュー。シーンカラーと深度が SRV 状態である前提
	 * @param allocator フレーム単位の定数割り当て器
	 */
	void Draw(Camera* camera, RenderView* view, FrameConstantAllocator* allocator);

	bool ReloadShaders(std::string& outError);
	Settings& GetSettings() { return settings_; }

#ifdef USE_IMGUI
	void RegisterDebugUI();
	void DrawImGui();
#endif

private:
	// 所有しない
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	FullscreenPass blurPass_;
	FullscreenPass copyPass_;
	std::unique_ptr<RenderTexture> target_;
	uint32_t width_ = 0;
	uint32_t height_ = 0;
	Settings settings_;
};
} // namespace KCE
