#pragma once
#include <cstdint>
#include <memory>
#include <string>

#include "graphics/postfx/FullscreenPass.h"
#include "math/Vector2.h"

namespace KCE
{
class DirectXCommon;
class FrameConstantAllocator;
class RenderTexture;
class SrvManager;

/**
 * @brief FXAA（画面のジャギーを後から均す）
 *
 * @details トーンマップ後の LDR の絵に掛ける。ポストプロセスの出力を一度
 *          ここの入力用ターゲットへ描かせてから、最終の出力先（エディタの
 *          レンダーターゲットかバックバッファ）へ均しながら書く。
 */
class FxaaRenderer
{
public:
	struct Settings
	{
		bool enabled = true;
		// 1ピクセルより細かい段差をどれだけ均すか。上げるほどぼける
		float subpixel = 0.75f;
		// 周りとの明るさの差がこの割合を超えたら縁とみなす
		float edgeThreshold = 0.166f;
		// 暗い所で縁とみなす最小の差
		float edgeThresholdMin = 0.0833f;
	};

	/** @brief Fxaa.PS.hlsl の cbuffer FxaaConstants と一致させること */
	struct ConstantsForGPU
	{
		Vector2 invTextureSize;
		float subpixel;
		float edgeThreshold;
		float edgeThresholdMin;
		float padding[3];
	};
	static_assert(sizeof(ConstantsForGPU) == 32, "FxaaConstants のサイズがシェーダー側と一致しません");

	~FxaaRenderer();

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height);
	void Resize(uint32_t width, uint32_t height);

	/** @brief このフレームで掛けるか */
	bool IsActive() const;

	/** @brief ポストプロセスの出力先にするターゲット */
	RenderTexture* GetInputTarget() const { return inputTarget_.get(); }

	/**
	 * @brief 入力用ターゲットの絵を均して出力先へ書く
	 * @param output 出力先。nullptr ならバックバッファ（描画可能な状態になっている前提）
	 * @param allocator フレーム単位の定数割り当て器
	 */
	void Apply(RenderTexture* output, FrameConstantAllocator* allocator);

	bool ReloadShaders(std::string& outError) { return pass_.ReloadShaders(outError); }
	Settings& GetSettings() { return settings_; }

#ifdef USE_IMGUI
	void RegisterDebugUI();
	void DrawImGui();
#endif

private:
	// 所有しない
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	FullscreenPass pass_;
	std::unique_ptr<RenderTexture> inputTarget_;
	uint32_t width_ = 0;
	uint32_t height_ = 0;
	Settings settings_;
};
} // namespace KCE
