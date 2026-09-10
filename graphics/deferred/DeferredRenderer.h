#pragma once
#include <memory>
#include <wrl.h>
#include <d3d12.h>

#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/MatrixFunc.h"

#include "GBuffer.h"
#include "GBufferPipeline.h"
#include "LightPassPipeline.h"

namespace KCE
{
class DirectXCommon;
class SrvManager;
class LightManager;
class ShadowMapManager;
class CameraManager;
class Camera;

/**
 * @brief ディファードレンダラークラス
 * @details G-Buffer、ライトパス、シャドウを統合管理
 */
class DeferredRenderer
{
public:
	// 最大ライト数
	static constexpr uint32_t kMaxSpotLights = 8;
	static constexpr uint32_t kMaxPointLights = 2;

	/**
	 * @brief トゥーン（NPR）の全体設定。LightPass.PS.hlsl の cbuffer ToonSettings と一致させる
	 * @details 素材ごとの値（効き具合・リムの強さ）は Material 側に持つ。
	 *          ここは画面全体で共通の塗り分け方。
	 */
	struct ToonSettingsForGPU
	{
		// 明部と暗部の境界（NdotL）
		float threshold = 0.5f;
		// 境界のぼかし幅
		float softness = 0.05f;
		// リムの鋭さ
		float rimPower = 4.0f;
		float padding = 0.0f;
		// 暗部に乗算する色（rgb）。黒で落とすとアニメ調の絵では濁るため、少し色味を残す
		Vector4 shadowTint = { 0.55f, 0.5f, 0.65f, 1.0f };
		// リムの色（rgb）
		Vector4 rimColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	};
	static_assert(sizeof(ToonSettingsForGPU) == 48, "ToonSettings のサイズがシェーダー側と一致しません");

	DeferredRenderer() = default;
	~DeferredRenderer();

	/** @brief トゥーンの全体設定を取得する（演出から動かすために公開） */
	ToonSettingsForGPU& GetToonSettings() { return toonSettings_; }

#ifdef USE_IMGUI
	/** @brief トゥーンの全体設定を調整するデバッグUIを登録する */
	void RegisterDebugUI();
	/** @brief デバッグUIを描画する */
	void DrawImGui();
#endif

	/**
	 * @brief 初期化する
	 * @details G-Buffer は所有しない。解像度ごとに必要になるため、
	 *          ビュー（RenderView）が持つ。ここが持つのは
	 *          全ビューで共有できるパイプラインステートと定数バッファだけ。
	 * @param dxCommon DirectXCommonへのポインタ
	 * @param srvManager SrvManagerへのポインタ
	 */
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	/**
	 * @brief G-Bufferパスを開始する
	 * @param gBuffer 描き込み先のG-Buffer
	 */
	void BeginGeometryPass(GBuffer* gBuffer);

	/**
	 * @brief G-Bufferパスを終了する
	 * @param gBuffer 対象のG-Buffer
	 */
	void EndGeometryPass(GBuffer* gBuffer);

	/**
	 * @brief ライトパスを実行する
	 * @param gBuffer 読み込むG-Buffer
	 * @param camera このビューを描くカメラ
	 * @param rtvHandle 出力先のRTVハンドル
	 * @param lightManager ライト管理
	 * @param shadowMapManager シャドウマップ管理
	 */
	void ExecuteLightPass(
		GBuffer* gBuffer,
		Camera* camera,
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
		LightManager* lightManager,
		ShadowMapManager* shadowMapManager
	);

	GBufferPipeline* GetGBufferPipeline() { return gBufferPipeline_.get(); }

private:
	void CreateCameraBuffer();
	void UpdateCameraBuffer(Camera* camera);
	void CreateLightBuffer();
	void UpdateLightBuffer(LightManager* lightManager, ShadowMapManager* shadowMapManager);

private:
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	std::unique_ptr<GBufferPipeline> gBufferPipeline_;
	std::unique_ptr<LightPassPipeline> lightPassPipeline_;

	// カメラデータ
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraBuffer_;
	struct CameraDataForGPU
	{
		Vector3 worldPos;
		float padding0;
		Matrix4x4 viewMatrix;
		Matrix4x4 projMatrix;
		Matrix4x4 invViewMatrix;
		Matrix4x4 invProjMatrix;
		float nearPlane;
		float farPlane;
		float padding1[2];
	};
	CameraDataForGPU* cameraData_ = nullptr;

	// スポットライトデータ（シェーダー用）
	struct SpotLightForGPU
	{
		Vector4 color;
		Vector3 position;
		float intensity;
		Vector3 direction;
		float distance;
		float decay;
		float cosAngle;
		float cosFalloffStart;
		int32_t shadowEnabled;
		float padding[4]; // 16バイトアライメント用パディング (全体サイズを16の倍数にするため)
		Matrix4x4 shadowViewProj;
	};

	// ポイントライトデータ（シェーダー用）
	struct PointLightForGPU
	{
		Vector4 color;
		Vector3 position;
		float intensity;
		float radius;
		float decay;
		int32_t shadowEnabled;
		float padding;
		Matrix4x4 shadowViewProj[6]; // 6面のキューブマップ
	};

	// ライトバッファ構造体
	struct LightBufferForGPU
	{
		int32_t numSpotLights;
		int32_t numPointLights;
		float padding[2];
		SpotLightForGPU spotLights[kMaxSpotLights];
		PointLightForGPU pointLights[kMaxPointLights];
	};

	Microsoft::WRL::ComPtr<ID3D12Resource> lightBuffer_;
	LightBufferForGPU* lightBufferData_ = nullptr;

	// 確認用に全オブジェクトへ適用する値（デバッグUI）
	float previewToonAmount_ = 1.0f;
	float previewRimStrength_ = 0.3f;
	float previewOutlineStrength_ = 1.0f;

	// トゥーン（NPR）の全体設定
	ToonSettingsForGPU toonSettings_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> toonBuffer_;
	ToonSettingsForGPU* toonData_ = nullptr;
};
} // namespace KCE
