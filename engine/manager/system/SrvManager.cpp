#include "SrvManager.h"

const uint32_t SrvManager::kMaxSRVCount = 512;

void SrvManager::Initialize(DirectXCommon* dxCommon)
{
	//引数で受け取ってメンバ変数に記録する
	dxCommon_ = dxCommon;

	//ディスクリプタヒープの生成
	descriptorHeap_ = dxCommon->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSRVCount, true);
	//ディスクリプタ１個分のサイズを取得して記録
	descriptorSize_ = dxCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

}

uint32_t SrvManager::Allocate()
{
	//上限に達していないか確認
	assert(useIndex_ < kMaxSRVCount);

	//returnする番号を一旦記録しておく
	int index = useIndex_;
	//今回のために番号を1進める
	useIndex_++;
	//上で記録した番号を返す
	return index;
}

void SrvManager::CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = UINT(mipLevels);

	//SRVの生成
	dxCommon_->GetDevice()->CreateShaderResourceView(
		pResource,
		&srvDesc,
		GetCPUDescriptorHandle(srvIndex)
	);
}

void SrvManager::CreateSRVforTexture2DCubeMap(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format,UINT mipLevels)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;	//unionがTextureCubeになったが内部パラメーターの意味はTexture2Dと同じ
	srvDesc.TextureCube.MipLevels = UINT_MAX;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	//SRVの生成
	dxCommon_->GetDevice()->CreateShaderResourceView(
		pResource,
		&srvDesc,
		GetCPUDescriptorHandle(srvIndex)
	);
}

void SrvManager::CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements,
                                              UINT structureByteStride)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.NumElements = numElements;
	srvDesc.Buffer.StructureByteStride = structureByteStride;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	//SRVの生成
	dxCommon_->GetDevice()->CreateShaderResourceView(
		pResource,
		&srvDesc,
		GetCPUDescriptorHandle(srvIndex)
	);
}

void SrvManager::PreDraw()
{
	//描画用のDescriptorHeapをセット
	ID3D12DescriptorHeap* descriptorHeaps[] = { descriptorHeap_.Get() };
	dxCommon_->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);
}

void SrvManager::SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex)
{
	//RootSignatureの設定
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(RootParameterIndex, GetGPUDescriptorHandle(srvIndex));
}

bool SrvManager::IsMaxSRVCount()
{
	return useIndex_ >= kMaxSRVCount;
}

D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize_ * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize_ * index);
	return handleGPU;
}
