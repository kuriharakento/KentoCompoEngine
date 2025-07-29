#include "TextureManager.h"

// system
#include "base/StringUtility.h"
#include "externals/DirectXTex/d3dx12.h"

// ImGuiで0番を使用するため、1番から開始
uint32_t TextureManager::kSRVIndexTop = 1;

//テクスチャマネージャーのインスタンス
TextureManager* TextureManager::instance_ = nullptr;

TextureManager* TextureManager::GetInstance()
{
	if (instance_ == nullptr)
	{
		instance_ = new TextureManager();
	}
	return instance_;
}

void TextureManager::Finalize()
{
	delete instance_;
	instance_ = nullptr;
}

void TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	textureDatas_.reserve(srvManager_->kMaxSRVCount);
}

void TextureManager::LoadTexture(const std::string& filePath)
{
	/*--------------[ 読み込み済みテクスチャを検索 ]-----------------*/
	if (textureDatas_.contains(filePath))
	{
		// 読み込み済みなら何もしない
		return;
	}

	// テクスチャ枚数上限チェック
	assert(!srvManager_->IsMaxSRVCount());

	/*--------------[ テクスチャファイルを読んでプログラムで扱えるようにする ]-----------------*/

	DirectX::ScratchImage image{};
	std::wstring filePathW = StringUtility::ConvertString(filePath);
	HRESULT hr;
	if (filePathW.ends_with(L".dds"))
	{
		// DDSファイルの場合
		hr = DirectX::LoadFromDDSFile(
			filePathW.c_str(),
			DirectX::DDS_FLAGS_NONE,
			nullptr,
			image
		);
	}
	else
	{
		hr = DirectX::LoadFromWICFile(
			filePathW.c_str(),
			DirectX::WIC_FLAGS_FORCE_SRGB,
			nullptr,
			image
		);
	}
	assert(SUCCEEDED(hr));

	/*--------------[ ミップマップの作成 ]-----------------*/

	DirectX::ScratchImage mipImages{};
	if (DirectX::IsCompressed(image.GetMetadata().format))
	{
		// 圧縮フォーマットならそのまま使うのでmoveする
		mipImages = std::move(image);
	}
	else
	{
		hr = DirectX::GenerateMipMaps(
			image.GetImages(),
			image.GetImageCount(),
			image.GetMetadata(),
			DirectX::TEX_FILTER_SRGB,
			0,
			mipImages
		);
	}
	assert(SUCCEEDED(hr));

	/*--------------[ テクスチャデータを追加 ]-----------------*/

	// 追加したテクスチャデータの参照を取得する
	TextureData& textureData = textureDatas_[filePath];

	/*--------------[ テクスチャデータの書き込み ]-----------------*/

	textureData.metadata = mipImages.GetMetadata();
	textureData.resource = dxCommon_->CreateTextureResource(textureData.metadata);
	textureData.intermediate = UploadTextureData(textureData.resource, mipImages);

	/*--------------[ ディスクリプタハンドルの計算 ]-----------------*/

	// テクスチャデータの要素数番号をSRVのインデックスとする
	uint32_t srvIndex = static_cast<uint32_t>(textureDatas_.size() - 1) + kSRVIndexTop;

	textureData.srvHandleCPU = GetSrvHandleCPU(srvIndex);
	textureData.srvHandleGPU = GetSrvHandleGPU(srvIndex);

	/*--------------[ SRVの生成 ]-----------------*/

	textureData.srvIndex = srvManager_->Allocate();

	if(textureData.metadata.IsCubemap())
	{
		// キューブマップテクスチャの場合はキューブマップ用のSRVを生成
		srvManager_->CreateSRVforTexture2DCubeMap(textureData.srvIndex, textureData.resource.Get(), textureData.metadata.format, static_cast<UINT>(textureData.metadata.mipLevels));
	}
	else
	{
		// 通常の2Dテクスチャの場合は2D用のSRVを生成
		srvManager_->CreateSRVforTexture2D(textureData.srvIndex, textureData.resource.Get(), textureData.metadata.format, static_cast<UINT>(textureData.metadata.mipLevels));
	}

	textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
	textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);

	/*--------------[ インデックス管理用マッピング ]-----------------*/

	filePathToIndex_[filePath] = textureData.srvIndex;
	indexToFilePath_[textureData.srvIndex] = filePath;
}

uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath)
{
	assert(filePathToIndex_.contains(filePath)); // 存在確認
	return filePathToIndex_[filePath];
}

const DirectX::TexMetadata& TextureManager::GetMetadata(uint32_t textureIndex)
{
	// インデックスがマッピング内に存在するか確認
	assert(indexToFilePath_.contains(textureIndex));
	const std::string& filePath = indexToFilePath_[textureIndex];
	return textureDatas_.at(filePath).metadata;
}

[[nodiscard]]
Microsoft::WRL::ComPtr<ID3D12Resource> TextureManager::UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource> texture, const DirectX::ScratchImage& mipImages)
{
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(dxCommon_->GetDevice(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
	uint64_t intermediateSize = GetRequiredIntermediateSize(texture.Get(), 0, UINT(subresources.size()));
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = dxCommon_->CreateBufferResource(intermediateSize);
	UpdateSubresources(dxCommon_->GetCommandList(), texture.Get(), intermediateResource.Get(), 0, 0, UINT(subresources.size()), subresources.data());
	// Textureへの転送後は利用できるよう、D3D12_RESOURCE_STATE_COPY_DESTからD3D12_RESOURCE_STATE_GENERIC_READへResourceStateを変更する
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
	return intermediateResource;
}
