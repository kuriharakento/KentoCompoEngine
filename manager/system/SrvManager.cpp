#include "SrvManager.h"

namespace KCE
{
// === 定数定義 ===
constexpr UINT kCubemapMostDetailedMip = 0;        // キューブマップの最詳細Mipレベル
constexpr float kCubemapMinLODClamp = 0.0f;        // キューブマップの最小LODクランプ値
constexpr UINT kDescriptorHeapCount = 1;           // セットするディスクリプタヒープ数

// 最大SRV数（512個：一般的なゲームで十分な数、GPUメモリ効率のバランス）
const uint32_t SrvManager::kMaxSRVCount = 512;

void SrvManager::Initialize(DirectXCommon* dxCommon)
{
	// DirectXCommonへのポインタを保存
	dxCommon_ = dxCommon;

	// SRV用ディスクリプタヒープの生成（シェーダーからアクセス可能）
	descriptorHeap_ = dxCommon->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSRVCount, true);
	// ディスクリプタ1個分のサイズを取得（GPU依存のため実行時に取得）
	descriptorSize_ = dxCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

}

uint32_t SrvManager::Allocate()
{
	// フリーリストに空きがあればそれを使う
	if (!freeList_.empty())
	{
		uint32_t index = freeList_.back();
		freeList_.pop_back();
		return index;
	}

	// 最大SRV数を超えていないかチェック（オーバーフロー防止）
	assert(useIndex_ < kMaxSRVCount);

	// 返却用にインデックスを保存
	int index = useIndex_;
	// 次回確保用にインデックスを進める
	useIndex_++;
	// 確保したインデックスを返す
	return index;
}

void SrvManager::Free(uint32_t index)
{
	freeList_.push_back(index);
}

uint32_t SrvManager::AllocateRange(uint32_t count)
{
	// 連続確保後も最大SRV数を超えないかチェック
	assert(useIndex_ + count <= kMaxSRVCount);

	// 連続するSRVの開始インデックスを保存
	uint32_t startIndex = useIndex_;
	// 指定数だけインデックスを進める
	useIndex_ += count;

	return startIndex;
}

void SrvManager::CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels)
{
	// SRV記述子の設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	// 2Dテクスチャとして設定
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = UINT(mipLevels);

	// 指定インデックスにSRVを作成
	dxCommon_->GetDevice()->CreateShaderResourceView(
		pResource,
		&srvDesc,
		GetCPUDescriptorHandle(srvIndex)
	);
}

void SrvManager::CreateSRVforTexture2DCubeMap(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format,UINT mipLevels)
{
	// SRV記述子の設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	// キューブマップテクスチャとして設定（6面の環境マップ用）
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = kCubemapMostDetailedMip;	// 最も詳細なMipレベルから使用
	srvDesc.TextureCube.MipLevels = UINT_MAX;	// 全てのMipレベルを使用
	srvDesc.TextureCube.ResourceMinLODClamp = kCubemapMinLODClamp;	// LODクランプなし
	// 指定インデックスにSRVを作成
	dxCommon_->GetDevice()->CreateShaderResourceView(
		pResource,
		&srvDesc,
		GetCPUDescriptorHandle(srvIndex)
	);
}

void SrvManager::CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements,
                                              UINT structureByteStride)
{
	// Structured Buffer用SRV記述子の設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;	// Structured Bufferは型指定不要
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	// バッファとして設定（構造体の配列をシェーダーで読み取り）
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.NumElements = numElements;	// バッファ内の要素数
	srvDesc.Buffer.StructureByteStride = structureByteStride;	// 1要素のサイズ
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	// 指定インデックスにSRVを作成
	dxCommon_->GetDevice()->CreateShaderResourceView(
		pResource,
		&srvDesc,
		GetCPUDescriptorHandle(srvIndex)
	);
}

void SrvManager::PreDraw()
{
	// 描画用ディスクリプタヒープをコマンドリストにセット
	ID3D12DescriptorHeap* descriptorHeaps[] = { descriptorHeap_.Get() };
	dxCommon_->GetCommandList()->SetDescriptorHeaps(kDescriptorHeapCount, descriptorHeaps);
}

void SrvManager::SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex)
{
	// 指定ルートパラメータにSRVをバインド
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(RootParameterIndex, GetGPUDescriptorHandle(srvIndex));
}

void SrvManager::SetGraphicsRootDescriptorTableRange(UINT RootParameterIndex, uint32_t startSrvIndex)
{
	// 連続するSRV範囲の開始位置をバインド
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(RootParameterIndex, GetGPUDescriptorHandle(startSrvIndex));
}

bool SrvManager::IsMaxSRVCount()
{
	// 現在のインデックスが最大数以上なら最大に達している
	return useIndex_ >= kMaxSRVCount;
}

D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t index)
{
	// ヒープの先頭からインデックス分のオフセットを計算
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize_ * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t index)
{
	// ヒープの先頭からインデックス分のオフセットを計算
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize_ * index);
	return handleGPU;
}
} // namespace KCE
