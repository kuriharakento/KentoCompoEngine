#include "SrvManager.h"
#include "base/Logger.h"
#include <algorithm>

namespace KCE
{
// === 定数定義 ===
constexpr UINT kCubemapMostDetailedMip = 0;        // キューブマップの最詳細Mipレベル
constexpr float kCubemapMinLODClamp = 0.0f;        // キューブマップの最小LODクランプ値
constexpr UINT kDescriptorHeapCount = 1;           // セットするディスクリプタヒープ数

// Particle GPU runtime may consume up to 20 descriptors per emitter. 4096
// keeps the 100-emitter stress preset viable while remaining far below the
// D3D12 shader-visible CBV/SRV/UAV heap hardware tier limits.
const uint32_t SrvManager::kMaxSRVCount = 4096;

void SrvManager::Initialize(DirectXCommon* dxCommon)
{
	// DirectXCommonへのポインタを保存
	dxCommon_ = dxCommon;

	// SRV用ディスクリプタヒープの生成（シェーダーからアクセス可能）
	descriptorHeap_ = dxCommon->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSRVCount, true);
	// ディスクリプタ1個分のサイズを取得（GPU依存のため実行時に取得）
	descriptorSize_ = dxCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	useIndex_ = 0;
	activeCount_ = 0;
	freeList_.clear();
	allocated_.assign(kMaxSRVCount, 0);

}

uint32_t SrvManager::Allocate()
{
	uint32_t index = kInvalidSrvIndex;
	if (!TryAllocate(index)) Logger::Log("SrvManager descriptor heap exhausted\n", Logger::LogLevel::Error);
	return index;
}

bool SrvManager::TryAllocate(uint32_t& outIndex)
{
	outIndex = kInvalidSrvIndex;
	if (!freeList_.empty())
	{
		const uint32_t index = freeList_.back();
		freeList_.pop_back();
		if (index >= kMaxSRVCount || allocated_[index] != 0) return false;
		allocated_[index] = 1;
		++activeCount_;
		outIndex = index;
		return true;
	}
	if (useIndex_ >= kMaxSRVCount) return false;
	outIndex = useIndex_++;
	allocated_[outIndex] = 1;
	++activeCount_;
	return true;
}

bool SrvManager::Free(uint32_t index)
{
	if (index >= kMaxSRVCount || allocated_.empty() || allocated_[index] == 0)
	{
		Logger::Log("SrvManager rejected invalid or duplicate descriptor free\n", Logger::LogLevel::Error);
		return false;
	}
	allocated_[index] = 0;
	--activeCount_;
	freeList_.push_back(index);
	return true;
}

uint32_t SrvManager::AllocateRange(uint32_t count)
{
	uint32_t startIndex = kInvalidSrvIndex;
	if (!TryAllocateRange(count, startIndex)) Logger::Log("SrvManager contiguous descriptor allocation failed\n", Logger::LogLevel::Error);
	return startIndex;
}

bool SrvManager::TryAllocateRange(uint32_t count, uint32_t& outStartIndex)
{
	outStartIndex = kInvalidSrvIndex;
	if (count == 0 || count > kMaxSRVCount || allocated_.size() != kMaxSRVCount) return false;

	uint32_t runStart = 0;
	uint32_t runLength = 0;
	for (uint32_t index = 0; index < kMaxSRVCount; ++index)
	{
		if (allocated_[index] == 0)
		{
			if (runLength == 0) runStart = index;
			if (++runLength == count)
			{
				for (uint32_t slot = runStart; slot < runStart + count; ++slot) allocated_[slot] = 1;
				activeCount_ += count;
				useIndex_ = (std::max)(useIndex_, runStart + count);
				freeList_.erase(std::remove_if(freeList_.begin(), freeList_.end(),
					[runStart, count](uint32_t slot) { return slot >= runStart && slot < runStart + count; }), freeList_.end());
				outStartIndex = runStart;
				return true;
			}
		}
		else
		{
			runLength = 0;
		}
	}
	return false;
}

bool SrvManager::FreeRange(uint32_t startIndex, uint32_t count)
{
	if (count == 0 || startIndex >= kMaxSRVCount || count > kMaxSRVCount - startIndex) return false;
	for (uint32_t index = startIndex; index < startIndex + count; ++index)
	{
		if (!IsAllocated(index))
		{
			Logger::Log("SrvManager rejected partially unallocated descriptor range free\n", Logger::LogLevel::Error);
			return false;
		}
	}
	for (uint32_t index = startIndex; index < startIndex + count; ++index)
	{
		allocated_[index] = 0;
		freeList_.push_back(index);
	}
	activeCount_ -= count;
	return true;
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
	return activeCount_ >= kMaxSRVCount;
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
