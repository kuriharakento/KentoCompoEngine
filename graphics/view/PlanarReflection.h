#pragma once
#include <cstdint>
#include <d3d12.h>
#include <string>

#include "graphics/postfx/FullscreenPass.h"
#include "graphics/view/RenderLayer.h"
#include "math/MatrixFunc.h"
#include "math/Vector3.h"

namespace KCE
{
class Camera;
class CameraManager;
class DirectXCommon;
class FrameConstantAllocator;
class GBuffer;
class ISubViewProvider;
class RenderView;
class SrvManager;

/**
 * @brief 床の平面反射
 *
 * @details 本編のカメラを床の高さで鏡に映したカメラでサブビューを描き、
 *          床のピクセル（高さが床と同じで上を向いている所）にだけ、反射率の分だけ混ぜる。
 *          - 反射する床は kReflectorLayer に入れる。反射の絵にはそのレイヤーを描かない
 *          - サブビューは本編の後に描かれるので、映るのは1フレーム前の絵
 *          - 鏡映したカメラの像は上下が反転しているので、合成時に反転して読む
 */
class PlanarReflection
{
public:
	/** @brief 反射する床を置くレイヤー。反射の絵には描かない */
	static constexpr uint32_t kReflectorLayerIndex = 2;
	static constexpr RenderLayerMask kReflectorLayer = MakeRenderLayerMask(kReflectorLayerIndex);

	struct Settings
	{
		bool enabled = false;
		// 床の高さ（ワールドの Y）
		float planeHeight = 0.0f;
		// 反射の強さ（床の色と反射を混ぜる割合の上限）
		float strength = 0.35f;
		// 浅い角度ほど強く映る度合い
		float fresnelPower = 4.0f;
		// 真上から見たときにも残す反射の割合
		float minReflectance = 0.2f;
		// 床とみなす高さのずれ
		float heightTolerance = 0.02f;
	};

	/** @brief Reflection.PS.hlsl の cbuffer ReflectionConstants と一致させること */
	struct ConstantsForGPU
	{
		Matrix4x4 invViewProjection;
		Vector3 cameraPosition;
		float planeHeight;
		float strength;
		float fresnelPower;
		float minReflectance;
		float heightTolerance;
	};
	static_assert(sizeof(ConstantsForGPU) == 96, "ReflectionConstants のサイズがシェーダー側と一致しません");

	~PlanarReflection();

	/**
	 * @brief サブビューとカメラを用意する
	 * @param provider サブビューの作成元。Finalize まで生きている前提
	 * @param width 画面の幅（反射はこの半分で描く）
	 * @param height 画面の高さ
	 */
	bool Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, ISubViewProvider* provider, CameraManager* cameraManager, uint32_t width, uint32_t height);

	/** @brief サブビューを消す */
	void Finalize();

	void Resize(uint32_t width, uint32_t height);

	/**
	 * @brief 本編のカメラを床で鏡映して反射用カメラへ写す
	 * @details サブビューを描く直前に呼ぶ。無効なら反射のサブビューも止める。
	 */
	void SyncCamera(const Camera* mainCamera);

	/**
	 * @brief 床のピクセルに反射を足す
	 * @details 深度は DEPTH_WRITE 状態で来る前提。読んだ後に戻す。
	 */
	void Composite(Camera* camera, GBuffer* gBuffer, D3D12_CPU_DESCRIPTOR_HANDLE sceneColorRtv, FrameConstantAllocator* allocator);

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
	ISubViewProvider* provider_ = nullptr;
	// provider_ が所有する
	RenderView* view_ = nullptr;
	// CameraManager が所有する
	Camera* camera_ = nullptr;

	FullscreenPass pass_;
	Settings settings_;
	// 反射の絵が一度でも描かれたか。描く前のターゲットは読めない状態のまま
	bool hasImage_ = false;
};
} // namespace KCE
