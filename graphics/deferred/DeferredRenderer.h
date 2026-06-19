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


class DirectXCommon;
class SrvManager;
class LightManager;
class ShadowMapManager;
class CameraManager;

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

	DeferredRenderer() = default;
	~DeferredRenderer() = default;

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height);
	void Resize(uint32_t width, uint32_t height);
	void BeginGeometryPass();
	void EndGeometryPass();
	void ExecuteLightPass(
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
		CameraManager* cameraManager,
		LightManager* lightManager,
		ShadowMapManager* shadowMapManager
	);

	GBufferPipeline* GetGBufferPipeline() { return gBufferPipeline_.get(); }
	GBuffer* GetGBuffer() { return gBuffer_.get(); }

private:
	void CreateCameraBuffer();
	void UpdateCameraBuffer(CameraManager* cameraManager);
	void CreateLightBuffer();
	void UpdateLightBuffer(LightManager* lightManager, ShadowMapManager* shadowMapManager);

private:
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	std::unique_ptr<GBuffer> gBuffer_;
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
};
