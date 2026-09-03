#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <cstdint>
#include <array>

namespace KCE
{
class DirectXCommon;
class SrvManager;

/**
 * @brief G-Bufferの構成
 * RT0: Albedo (RGB) + Metallic (A) - RGBA8
 * RT1: Normal (RGB, encoded) - RGB10A2
 * RT2: Roughness (R) + AO (G) + Reserved (BA) - RGBA8
 * RT3: Emissive Color (RGB) + Intensity (A) - RGBA8
 */
namespace GBufferIndex {
	constexpr uint32_t Albedo = 0;
	constexpr uint32_t Normal = 1;
	constexpr uint32_t Material = 2;
	constexpr uint32_t Emissive = 3;
	constexpr uint32_t Count = 4;
}

/**
 * @brief G-Bufferクラス
 * @details ディファードレンダリング用のジオメトリバッファを管理
 */
class GBuffer
{
public:
	GBuffer() = default;
	~GBuffer();

	/**
	 * @brief 初期化
	 * @param dxCommon DirectXCommonへのポインタ
	 * @param srvManager SrvManagerへのポインタ
	 * @param width 幅
	 * @param height 高さ
	 */
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height);

	/**
	 * @brief G-Bufferをリサイズする。
	 * @param width 新しい幅
	 * @param height 新しい高さ
	 */
	void Resize(uint32_t width, uint32_t height);

	/**
	 * @brief G-Bufferパスの開始
	 * @details レンダーターゲットと深度バッファを設定
	 */
	void BeginGeometryPass();

	/**
	 * @brief G-Bufferパスの終了
	 * @details リソースバリアを切り替え
	 */
	void EndGeometryPass();

	/**
	 * @brief G-BufferをシェーダーリソースとしてバインドするためのSRVインデックスを取得
	 * @param index G-Bufferのインデックス
	 * @return SRVインデックス
	 */
	uint32_t GetSRVIndex(uint32_t index) const;

	/**
	 * @brief 深度バッファのDSVハンドルを取得
	 */
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const { return dsvHandle_; }

	void TransitionDepthToDepthWrite();
	void TransitionDepthToSRV();

	/**
	 * @brief 深度バッファのSRVインデックスを取得
	 * @return SRVインデックス
	 */
	uint32_t GetDepthSRVIndex() const { return depthSrvIndex_; }

	/**
	 * @brief G-Bufferの幅を取得
	 */
	uint32_t GetWidth() const { return width_; }

	/**
	 * @brief G-Bufferの高さを取得
	 */
	uint32_t GetHeight() const { return height_; }

private:
	void CreateRenderTargets();
	void CreateDepthBuffer();
	void CreateDescriptors();

private:
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	uint32_t width_ = 0;
	uint32_t height_ = 0;

	// G-Bufferレンダーターゲット
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, GBufferIndex::Count> renderTargets_;
	std::array<uint32_t, GBufferIndex::Count> srvIndices_{};

	// RTV用ディスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
	uint32_t rtvDescriptorSize_ = 0;
	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, GBufferIndex::Count> rtvHandles_{};

	// 深度バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> depthBuffer_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{};
	uint32_t depthSrvIndex_ = 0;

	// フォーマット
	static constexpr DXGI_FORMAT kAlbedoFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	static constexpr DXGI_FORMAT kNormalFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
	static constexpr DXGI_FORMAT kMaterialFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	static constexpr DXGI_FORMAT kEmissiveFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	// static constexpr DXGI_FORMAT kDepthFormat = DXGI_FORMAT_D32_FLOAT; // D32_FLOATはForward Passと不一致のため廃止

	// 現在の深度バッファの状態
	D3D12_RESOURCE_STATES currentDepthState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
};
} // namespace KCE
