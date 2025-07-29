#pragma once
#include "base/DirectXCommon.h"

class SrvManager
{
public:
	//初期化
	void Initialize(DirectXCommon* dxCommon);

	//確保
	uint32_t Allocate();

	//SRV生成（テクスチャ用）
	void CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format,UINT mipLevels);
	void CreateSRVforTexture2DCubeMap(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels);
	//SRV生成（Structured Buffer用）
	void CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

	//描画前処理
	void PreDraw();

	//
	void SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex);

	//最大枚数なのか
	bool IsMaxSRVCount();

public: //アクセッサ

	//ヒープの取得
	ID3D12DescriptorHeap* GetSrvHeap() { return descriptorHeap_.Get(); }

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

public:
	//最大SRV数
	static const uint32_t kMaxSRVCount;

private:
	DirectXCommon* dxCommon_ = nullptr;

	//SRVのディスクリプタサイズ
	uint32_t descriptorSize_;
	//SRVのディスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
	//次に使用するSRVインデックス
	uint32_t useIndex_ = 0;


};

