#pragma once
#include <cstdint>
#include <d3d12.h>
#include <memory>
#include <string>

#include "graphics/postfx/FullscreenPass.h"
#include "math/MatrixFunc.h"
#include "math/Vector2.h"
#include "math/Vector3.h"

namespace KCE
{
class BeamRenderer;
class Camera;
class DirectXCommon;
class FrameConstantAllocator;
class GBuffer;
class LightManager;
class RenderTexture;
class ShadowMapManager;
class SrvManager;

/**
 * @brief スポットライトのボリュメトリック（レイマーチで空気中の光の筋を出す）
 *
 * @details 深度から各ピクセルまでのカメラの光線を、半分の解像度で数十歩ずつ進み、
 *          歩ごとに「スポットのコーンの中か」「シャドウマップで遮られていないか」を見て、
 *          Henyey-Greenstein の位相関数で散乱した光を積む。
 *          結果は深度を見ながら（バイラテラルで）拡大してシーンへ足し込む。
 *          - 照らすのは BeamRenderer でビームを出しているライトの先頭 kMaxLights 本
 *          - 歩き始めの位置を画面の模様でずらし、段々の縞をノイズに変えている
 */
class VolumetricLightRenderer
{
public:
	static constexpr uint32_t kMaxLights = 4;

	struct Settings
	{
		bool enabled = false;
		// 空気の濃さ。上げるほど光の筋が濃くなり、奥が霞む
		float density = 0.04f;
		// 位相関数の偏り（-1〜1）。正にするとライトの方を向いたときに明るい
		float anisotropy = 0.4f;
		// 1本の光線を何歩で進むか
		int32_t stepCount = 48;
		// 光線を進める最大距離
		float maxDistance = 40.0f;
		// 光の強さの倍率。位相関数は 1/(4π) で正規化されていて素のままだと薄いので、大きめにしておく
		float intensity = 10.0f;
	};

	/** @brief VolumetricMarch.PS.hlsl の struct VolumeLight と一致させること */
	struct LightForGPU
	{
		Vector3 position;
		float range;
		Vector3 direction;
		float cosAngle;
		Vector3 color;
		float cosFalloffStart;
		float decay;
		int32_t shadowEnabled;
		float padding[2];
		Matrix4x4 shadowViewProj;
	};
	static_assert(sizeof(LightForGPU) == 128, "VolumeLight のサイズがシェーダー側と一致しません");

	/** @brief VolumetricMarch / VolumetricComposite の cbuffer VolumetricConstants と一致させること */
	struct ConstantsForGPU
	{
		Matrix4x4 invViewProjection;
		Vector3 cameraPosition;
		float density;
		float anisotropy;
		float maxDistance;
		int32_t stepCount;
		int32_t lightCount;
		Vector2 invFullSize;
		Vector2 invHalfSize;
		LightForGPU lights[kMaxLights];
	};
	static_assert(sizeof(ConstantsForGPU) == 112 + sizeof(LightForGPU) * kMaxLights, "VolumetricConstants のサイズがシェーダー側と一致しません");

	~VolumetricLightRenderer();

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height);
	void Resize(uint32_t width, uint32_t height);

	/**
	 * @brief 光の筋を描いてシーンへ足す
	 * @details 深度は DEPTH_WRITE 状態で来る前提。読んだ後に戻し、
	 *          出力先とビューポートもシーンのものに戻して返る。
	 * @param viewWidth シーンの幅
	 * @param viewHeight シーンの高さ
	 * @param beamRenderer どのライトを照らすかを見る。nullptr なら全部のスポットが対象
	 */
	void Draw(Camera* camera, GBuffer* gBuffer, D3D12_CPU_DESCRIPTOR_HANDLE sceneColorRtv, uint32_t viewWidth, uint32_t viewHeight,
		LightManager* lightManager, ShadowMapManager* shadowMapManager, BeamRenderer* beamRenderer, FrameConstantAllocator* allocator);

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

	FullscreenPass marchPass_;
	FullscreenPass compositePass_;
	// 半分の解像度の光の筋
	std::unique_ptr<RenderTexture> halfTarget_;
	uint32_t halfWidth_ = 0;
	uint32_t halfHeight_ = 0;
	Settings settings_;
};
} // namespace KCE
