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


class DirectXCommon
{
public: //メンバ関数
	//初期化
	void Initialize(WinApp *winApp);

	//描画前処理
	void PreDraw();

	//描画後処理
	void PostDraw();

	/// \brief バッファリソースの生成
	/// \param sizeInBytes 
	/// \return 
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

	/// \brief テクスチャリソースの生成
	/// \param metadata 
	/// \return 
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);

	/// \brief テクスチャリソースの転送
	/// \param texture 
	/// \param mipImages 
	/// \return 
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource> texture, const DirectX::ScratchImage& mipImages);

	/// \brief シェーダーのコンパイル
	/// \param filePath 
	/// \param profile 
	/// \return 
	Microsoft::WRL::ComPtr<IDxcBlob> CompileSharder(const std::wstring& filePath, const wchar_t* profile);

	/**
	 * \brief ディスクリプタヒープの生成
	 * \param heapType 
	 * \param numDescriptor 
	 * \param shaderVisible 
	 * \return 
	 */
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptor, bool shaderVisible);

	void CreateSamplerHeap();

public://アクセッサ
	/// \brief デバイスの取得
	/// \return 
	ID3D12Device* GetDevice() { return device_.Get(); }

	//コマンドリストの取得
	ID3D12GraphicsCommandList* GetCommandList() { return commandList_.Get(); }

	//DXCコンパイラの取得
	IDxcCompiler3* GetDXCCompiler() { return dxcCompiler_.Get(); }

	//DXCユーティリティの取得
	IDxcUtils* GetDXCUtils() { return dxcUtils_.Get(); }

	//インクルードハンドラの取得
	IDxcIncludeHandler* GetIncludeHandler() { return includeHandler_.Get(); }

	//各種ディスクリプヒープの取得
	ID3D12DescriptorHeap* GetRTVDescriptorHeap() { return rtvDescriptorHeap_.Get(); }
	ID3D12DescriptorHeap* GetDSVDescriptorHeap() { return dsvDescriptorHeap_.Get(); }
	ID3D12DescriptorHeap* GetSamplerHeap() { return samplerHeap_.Get(); }	

	//各種ディスクリプタサイズの取得
	uint32_t GetDescriptorSizeRTV() { return descriptorSizeRTV_; }
	uint32_t GetDescriptorSizeDSV() { return descriptorSizeDSV_; }

	//バックバッファの取得
	size_t GetBackBufferCount() { return swapChainResources_.size(); }

	/// \brief CPUのDescriptorHandleを取得
	/// \param descriptorHeap 
	/// \param descriptorSize 
	/// \param index 
	/// \return 
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);

	/// \brief GPUのDescriptorHandleを取得
	/// \param descriptorHeap 
	/// \param descriptorSize 
	/// \param index 
	/// \return 
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);

	// Sampler用ディスクリプタヒープのサイズを取得する関数
	D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerDescriptorHandle() const
	{
		assert(samplerHeap_ != nullptr && "Sampler Heap is not initialized!");
		return samplerHeap_->GetGPUDescriptorHandleForHeapStart();
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() { return dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart(); }
	
private: //メンバ関数
	/// \brief デバイスの初期化
	void InitializeDevice();
	/// \brief コマンド関連の初期化
	void InitializeCommand();
	/// \brief スワップチェインの生成
	void CreateSwapChain();
	//// \brief 深度バッファの生成
	void CreateDepthBuffer();
	// 各種ディスクリプタの生成
	void CreateDescriptorHeap();
	/// \brief レンダーターゲットビューの生成
	void CreateRenderTargetView();
	//深度ステンシルビューの初期化
	void CreateDepthStencilView();
	/// \brief フェンスの生成
	void CreateFence();
	/// \brief ビューポート矩形の初期化
	void InitializeViewPort();
	/// \brief シザリング矩形の初期化
	void InitializeScissorRect();
	// DXCコンパイラの初期化
	void InitializeDXCCompiler();
	/// \brief FPS固定初期化
	void InitializeFixFPS();
	/// \brief FPS固定更新
	void UpdateFixFPS();	

	
private: //メンバ変数
	//WindowsAPIの
	WinApp* winApp_ = nullptr;

	//DirectX12デバイス
	Microsoft::WRL::ComPtr<ID3D12Device> device_;
	//DXGIファクトリー
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;
	//コマンドアロケータ
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_ = nullptr;
	//コマンドリスト
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_ = nullptr;
	//コマンドキュー
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_ = nullptr;
	//スワップチェイン
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_ = nullptr;
	//レンダーテクスチャ
	Microsoft::WRL::ComPtr<ID3D12Resource> renderTexture_ = nullptr;
	//スワップチェインリソース
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> swapChainResources_;
	//深度バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> depthBuffer_ = nullptr;
	//ディスクリプタヒープ
	//RTV
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_ = nullptr;
	//DSV
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_ = nullptr;
	//Sampler用のヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> samplerHeap_ = nullptr;
	//RTVを2つ作るのでディスクリプタを２つ用意
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[2]{};
	//ディスクリプタサイズ
	uint32_t descriptorSizeRTV_;
	uint32_t descriptorSizeDSV_;
	//フェンス
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_ = nullptr;
	uint64_t fenceValue_ = 0;
	//HANDLE fenceEvent_;
	//ビューポート
	D3D12_VIEWPORT viewport_{};
	//シザー矩形
	D3D12_RECT scissorRect_{};
	//DXCユーティリティ
	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_ = nullptr;
	//DXCコンパイラ
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_ = nullptr;
	//インクルードハンドラ
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_ = nullptr;
	//リソースバリア
	D3D12_RESOURCE_BARRIER barrier_{};
	std::chrono::steady_clock::time_point reference_;
	//レンダーテクスチャのクリア値
	D3D12_CLEAR_VALUE clearValue_;
};

