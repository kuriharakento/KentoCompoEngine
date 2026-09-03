#pragma once
#include <array>
#include <chrono>
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <string>
#include <wrl.h>

#include "base/WinApp.h"
#include "externals/DirectXTex/DirectXTex.h"
#include "math/Vector4.h"

namespace KCE
{
/**
 * @brief DirectX12の共通機能を管理するクラス
 */
class DirectXCommon
{
public:
	/**
	 * @brief 初期化
	 * @param winApp WindowsAPIクラスへのポインタ
	 */
	void Initialize(WinApp *winApp);

	/**
	 * @brief 描画前処理
	 */
	void PreDraw();

	/**
	 * @brief 描画後処理
	 */
	void PostDraw();

	/**
	 * @brief バッファリソースの生成
	 * @param sizeInBytes バッファサイズ（バイト単位）
	 * @return 生成したバッファリソース
	 */
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

	/**
	 * @brief テクスチャリソースの生成
	 * @param metadata テクスチャメタデータ
	 * @return 生成したテクスチャリソース
	 */
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);

	/**
	 * @brief テクスチャデータの転送
	 * @param texture 転送先テクスチャリソース
	 * @param mipImages ミップマップ画像データ
	 * @return 中間リソース
	 */
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource> texture, const DirectX::ScratchImage& mipImages);

	/**
	 * @brief シェーダーのコンパイル
	 * @param filePath シェーダーファイルパス
	 * @param profile シェーダープロファイル
	 * @return コンパイル済みシェーダーブロブ
	 */
	Microsoft::WRL::ComPtr<IDxcBlob> CompileSharder(const std::wstring& filePath, const wchar_t* profile);

	/**
	 * @brief ディスクリプタヒープの生成
	 * @param heapType ヒープタイプ
	 * @param numDescriptor ディスクリプタ数
	 * @param shaderVisible シェーダーから参照可能かどうか
	 * @return 生成したディスクリプタヒープ
	 */
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptor, bool shaderVisible);

	/**
	 * @brief サンプラー用ディスクリプタヒープの生成
	 */
	void CreateSamplerHeap();

	/**
	 * @brief コマンドの実行と完了待ち
	 * @details 現在のコマンドリストをクローズして実行し、GPUの完了を待機する。
	 *          完了後は次フレーム用のリセットまで行う。
	 */
	void ExecuteAndWait();
	uint64_t GetLastSubmittedFenceValue() const { return fenceValue_; }
	uint64_t GetNextFenceValue() const { return fenceValue_ + 1; }
	uint64_t GetCompletedFenceValue() const { return fence_ ? fence_->GetCompletedValue() : 0; }
	/** Direct command queueのGPU timestamp周波数を取得する。 */
	bool GetTimestampFrequency(uint64_t& frequency) const;

	/**
	 * @brief ウィンドウサイズ変更に伴うリサイズ処理を行う。
	 * @param width 新しい幅
	 * @param height 新しい高さ
	 */
	void Resize(uint32_t width, uint32_t height);

	/**
	 * @brief フルスクリーン状態を設定する。
	 * @param fullscreen フルスクリーンならtrue、ウィンドウモードならfalse
	 */
	void SetFullscreen(bool fullscreen);

	/**
	 * @brief フルスクリーン状態かどうかを取得する。
	 */
	bool IsFullscreen() const;
	bool IsVSyncEnabled() const { return vsyncEnabled_; }
	const std::string& GetAdapterName() const { return adapterName_; }
	const std::string& GetDriverVersion() const { return driverVersion_; }

public:
	/**
	 * @brief デバイスを取得
	 * @return デバイスへのポインタ
	 */
	ID3D12Device* GetDevice() { return device_.Get(); }

	/**
	 * @brief コマンドリストを取得
	 * @return コマンドリストへのポインタ
	 */
	ID3D12GraphicsCommandList* GetCommandList() { return commandList_.Get(); }

	/**
	 * @brief DXCコンパイラを取得
	 * @return DXCコンパイラへのポインタ
	 */
	IDxcCompiler3* GetDXCCompiler() { return dxcCompiler_.Get(); }

	/**
	 * @brief DXCユーティリティを取得
	 * @return DXCユーティリティへのポインタ
	 */
	IDxcUtils* GetDXCUtils() { return dxcUtils_.Get(); }

	/**
	 * @brief インクルードハンドラを取得
	 * @return インクルードハンドラへのポインタ
	 */
	IDxcIncludeHandler* GetIncludeHandler() { return includeHandler_.Get(); }

	/**
	 * @brief RTV用ディスクリプタヒープを取得
	 * @return RTV用ディスクリプタヒープへのポインタ
	 */
	ID3D12DescriptorHeap* GetRTVDescriptorHeap() { return rtvDescriptorHeap_.Get(); }

	/**
	 * @brief DSV用ディスクリプタヒープを取得
	 * @return DSV用ディスクリプタヒープへのポインタ
	 */
	ID3D12DescriptorHeap* GetDSVDescriptorHeap() { return dsvDescriptorHeap_.Get(); }

	/**
	 * @brief サンプラー用ディスクリプタヒープを取得
	 * @return サンプラー用ディスクリプタヒープへのポインタ
	 */
	ID3D12DescriptorHeap* GetSamplerHeap() { return samplerHeap_.Get(); }	

	/**
	 * @brief RTVのディスクリプタサイズを取得
	 * @return RTVのディスクリプタサイズ
	 */
	uint32_t GetDescriptorSizeRTV() { return descriptorSizeRTV_; }

	/**
	 * @brief DSVのディスクリプタサイズを取得
	 * @return DSVのディスクリプタサイズ
	 */
	uint32_t GetDescriptorSizeDSV() { return descriptorSizeDSV_; }

	/**
	 * @brief バックバッファの数を取得
	 * @return バックバッファの数
	 */
	size_t GetBackBufferCount() { return swapChainResources_.size(); }

	/**
	 * @brief 現在のバックバッファインデックスを取得
	 * @return 現在のバックバッファインデックス
	 */
	UINT GetCurrentBackBufferIndex() { return swapChain_->GetCurrentBackBufferIndex(); }

	/**
	 * @brief CPUディスクリプタハンドルを取得
	 * @param descriptorHeap ディスクリプタヒープ
	 * @param descriptorSize ディスクリプタサイズ
	 * @param index インデックス
	 * @return CPUディスクリプタハンドル
	 */
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);

	/**
	 * @brief GPUディスクリプタハンドルを取得
	 * @param descriptorHeap ディスクリプタヒープ
	 * @param descriptorSize ディスクリプタサイズ
	 * @param index インデックス
	 * @return GPUディスクリプタハンドル
	 */
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);

	/**
	 * @brief サンプラーディスクリプタハンドルを取得
	 * @return サンプラーのGPUディスクリプタハンドル
	 */
	D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerDescriptorHandle() const
	{
		assert(samplerHeap_ != nullptr && "Sampler Heap is not initialized!");
		return samplerHeap_->GetGPUDescriptorHandleForHeapStart();
	}

	/**
	 * @brief DSVハンドルを取得
	 * @return DSVのCPUディスクリプタハンドル
	 */
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() { return dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart(); }
	
private:
	/**
	 * @brief デバイスの初期化
	 */
	void InitializeDevice();

	/**
	 * @brief コマンド関連の初期化
	 */
	void InitializeCommand();

	/**
	 * @brief スワップチェインの生成
	 */
	void CreateSwapChain();

	/**
	 * @brief 深度バッファの生成
	 */
	void CreateDepthBuffer();

	/**
	 * @brief 各種ディスクリプタヒープの生成
	 */
	void CreateDescriptorHeap();

	/**
	 * @brief レンダーターゲットビューの生成
	 */
	void CreateRenderTargetView();

	/**
	 * @brief 深度ステンシルビューの初期化
	 */
	void CreateDepthStencilView();

	/**
	 * @brief フェンスの生成
	 */
	void CreateFence();

	/**
	 * @brief ビューポート矩形の初期化
	 */
	void InitializeViewPort();

	/**
	 * @brief シザリング矩形の初期化
	 */
	void InitializeScissorRect();

	/**
	 * @brief DXCコンパイラの初期化
	 */
	void InitializeDXCCompiler();

	/**
	 * @brief FPS固定初期化
	 */
	void InitializeFixFPS();

	/**
	 * @brief FPS固定更新
	 */
	void UpdateFixFPS();	

	
private:
	// WindowsAPIへのポインタ
	WinApp* winApp_ = nullptr;
	// DirectX12デバイス
	Microsoft::WRL::ComPtr<ID3D12Device> device_;
	// DXGIファクトリー
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;
	// コマンドアロケータ
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_ = nullptr;
	// コマンドリスト
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_ = nullptr;
	// コマンドキュー
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_ = nullptr;
	// スワップチェイン
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_ = nullptr;
	// レンダーテクスチャ
	Microsoft::WRL::ComPtr<ID3D12Resource> renderTexture_ = nullptr;
	// スワップチェインリソース
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> swapChainResources_;
	// 深度バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> depthBuffer_ = nullptr;
	// RTV用ディスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_ = nullptr;
	// DSV用ディスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_ = nullptr;
	// サンプラー用ディスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> samplerHeap_ = nullptr;
	// RTVハンドル（ダブルバッファ用に2つ）
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[2]{};
	// RTVのディスクリプタサイズ
	uint32_t descriptorSizeRTV_;
	// DSVのディスクリプタサイズ
	uint32_t descriptorSizeDSV_;
	// フェンス
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_ = nullptr;
	// フェンス値
	uint64_t fenceValue_ = 0;
	// ビューポート
	D3D12_VIEWPORT viewport_{};
	// シザー矩形
	D3D12_RECT scissorRect_{};
	// DXCユーティリティ
	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_ = nullptr;
	// DXCコンパイラ
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_ = nullptr;
	// インクルードハンドラ
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_ = nullptr;
	// リソースバリア
	D3D12_RESOURCE_BARRIER barrier_{};
	// フルスクリーン切り替え時の状態保存用
	DWORD savedWindowStyle_ = 0;
	WINDOWPLACEMENT savedWindowPlacement_ = { sizeof(WINDOWPLACEMENT) };
	bool isFullscreen_ = false;
	bool vsyncEnabled_ = true;
	std::string adapterName_;
	std::string driverVersion_ = "unknown";
	// FPS固定用の基準時間
	std::chrono::steady_clock::time_point reference_;
	// レンダーテクスチャのクリア値
	D3D12_CLEAR_VALUE clearValue_;
};
} // namespace KCE
