#include "TextureManager.h"

#include <algorithm>
#include <filesystem>
#include <iostream>

// system
#include "base/PathManager.h"
#include "base/StringUtility.h"
#include "externals/DirectXTex/d3dx12.h"

namespace KCE
{
// SRVインデックスの開始番号の実体（ImGuiが0番を使用するため、1番から開始）
uint32_t TextureManager::kSRVIndexTop = 1;

// シングルトンインスタンスの実体
std::unique_ptr<TextureManager> TextureManager::instance_ = nullptr;

TextureManager* TextureManager::GetInstance()
{
	// インスタンスが存在しない場合は生成
	if (instance_ == nullptr)
	{
		instance_ = std::make_unique<TextureManager>();
	}
	return instance_.get();
}

void TextureManager::Finalize()
{
	// シングルトンインスタンスを解放
	instance_.reset();
}

void TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	// 引数をメンバ変数に記録
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	// テクスチャデータの領域を予約
	textureDatas_.reserve(srvManager_->kMaxSRVCount);
}

void TextureManager::LoadTexture(const std::string& filePath)
{
	LoadTextureInternal(filePath, true);
}

void TextureManager::LoadTextureLinear(const std::string& filePath)
{
	LoadTextureInternal(filePath, false);
}

void TextureManager::LoadTextureInternal(const std::string& filePath, bool forceSrgb)
{
	// パスを正規化（重複読み込み防止用）
	std::string normalizedPath = NormalizePath(filePath) + (forceSrgb ? "" : "|linear");

	/*--------------[ 読み込み済みテクスチャを検索 ]-----------------*/
	if (textureDatas_.contains(normalizedPath))
	{
		// 読み込み済みなら何もしない
		return;
	}

	// テクスチャ枚数上限チェック
	assert(!srvManager_->IsMaxSRVCount());

	/*--------------[ テクスチャファイルを読み込み ]-----------------*/

	DirectX::ScratchImage image{};

	auto resolvedOpt = ResolveTexturePath(filePath);
	if (!resolvedOpt.has_value())
	{
		// assert が無効な Release でも optional を不正参照しない。
		// 呼び出し側は CheckTextureExists() で事前検証できる。
		std::cerr << "TextureManager::LoadTexture: texture not found: " << filePath << '\n';
		return;
	}
	std::wstring targetPath = resolvedOpt->wstring();

	HRESULT hr;

	// ファイル形式に応じて読み込み方法を変更
	if (targetPath.ends_with(L".dds"))
	{
		// DDSファイルの場合
		hr = DirectX::LoadFromDDSFile(
			targetPath.c_str(),
			DirectX::DDS_FLAGS_NONE,
			nullptr,
			image
		);
	}
	else
	{
		// WICファイル（PNG, JPG等）の場合
		hr = DirectX::LoadFromWICFile(
			targetPath.c_str(),
			forceSrgb ? DirectX::WIC_FLAGS_FORCE_SRGB : DirectX::WIC_FLAGS_IGNORE_SRGB,
			nullptr,
			image
		);
	}
	if (FAILED(hr))
	{
		std::cerr << "TextureManager::LoadTexture: decode failed: " << filePath << '\n';
		return;
	}
	if (!forceSrgb)
	{
		image.OverrideFormat(DirectX::MakeLinear(image.GetMetadata().format));
	}

	/*--------------[ ミップマップの作成 ]-----------------*/

	DirectX::ScratchImage mipImages{};
	const auto& sourceMetadata = image.GetMetadata();
	if (DirectX::IsCompressed(sourceMetadata.format) ||
		(sourceMetadata.width == 1 && sourceMetadata.height == 1))
	{
		// 圧縮済み、またはミップを生成できない1x1画像はそのまま使用する。
		mipImages = std::move(image);
	}
	else
	{
		// 非圧縮フォーマットの場合はミップマップを生成
		hr = DirectX::GenerateMipMaps(
			image.GetImages(),
			image.GetImageCount(),
			image.GetMetadata(),
			forceSrgb ? DirectX::TEX_FILTER_SRGB : DirectX::TEX_FILTER_DEFAULT,
			0,
			mipImages
		);
	}
	if (FAILED(hr))
	{
		std::cerr << "TextureManager::LoadTexture: mip generation failed: " << filePath << '\n';
		return;
	}

	/*--------------[ テクスチャデータを追加 ]-----------------*/

	uint32_t allocatedSrvIndex = SrvManager::kInvalidSrvIndex;
	if (!srvManager_->TryAllocate(allocatedSrvIndex))
	{
		std::cerr << "TextureManager::LoadTexture: descriptor heap exhausted\n";
		return;
	}

	// All fallible CPU-side work has succeeded. Publish the cache entry only
	// after a descriptor has also been reserved, so failed loads cannot leave a
	// half-initialized texture that later renderers mistake for valid data.
	TextureData& textureData = textureDatas_[normalizedPath];

	/*--------------[ テクスチャデータの書き込み ]-----------------*/

	textureData.metadata = mipImages.GetMetadata();
	textureData.resource = dxCommon_->CreateTextureResource(textureData.metadata);
	// 中間リソースをリストに追加して保持（後でまとめて解放）
	intermediateResources_.push_back(UploadTextureData(textureData.resource, mipImages));

	/*--------------[ SRVの生成 ]-----------------*/

	textureData.srvIndex = allocatedSrvIndex;

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

	// ディスクリプタハンドルを更新
	textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
	textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);

	/*--------------[ インデックス管理用マッピング ]-----------------*/

	filePathToIndex_[normalizedPath] = textureData.srvIndex;
	indexToFilePath_[textureData.srvIndex] = normalizedPath;
}

void TextureManager::ClearIntermediateResources()
{
	// 中間リソースを一括解放
	intermediateResources_.clear();
}

uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath)
{
	uint32_t index = SrvManager::kInvalidSrvIndex;
	const bool found = TryGetTextureIndexByFilePath(filePath, index);
	assert(found);
	return index;
}

uint32_t TextureManager::GetLinearTextureIndexByFilePath(const std::string& filePath)
{
	const auto it = filePathToIndex_.find(NormalizePath(filePath) + "|linear");
	assert(it != filePathToIndex_.end());
	return it->second;
}

bool TextureManager::TryGetTextureIndexByFilePath(const std::string& filePath, uint32_t& outIndex) const
{
	outIndex = SrvManager::kInvalidSrvIndex;
	const auto it = filePathToIndex_.find(NormalizePath(filePath));
	if (it == filePathToIndex_.end()) return false;
	outIndex = it->second;
	return srvManager_ && srvManager_->IsAllocated(outIndex);
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
	// サブリソースデータを準備
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(dxCommon_->GetDevice(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);

	// 中間バッファのサイズを計算
	uint64_t intermediateSize = GetRequiredIntermediateSize(texture.Get(), 0, UINT(subresources.size()));

	// 中間バッファを作成
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = dxCommon_->CreateBufferResource(intermediateSize);

	// サブリソースを更新
	UpdateSubresources(dxCommon_->GetCommandList(), texture.Get(), intermediateResource.Get(), 0, 0, UINT(subresources.size()), subresources.data());

	// テクスチャへの転送後は利用できるようにリソースステートを変更
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

std::string TextureManager::NormalizePath(const std::string& filePath) const
{
	// 相対パスを正規化（./ や ../ を解決）
	std::filesystem::path p(filePath);
	std::filesystem::path normalized = p.lexically_normal();

	// スラッシュ区切りに統一
	std::string result = normalized.generic_string();

	// 小文字に変換（Windows用: 大文字小文字を同一視）
	std::transform(result.begin(), result.end(), result.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	return result;
}

bool TextureManager::IsTextureLoaded(const std::string& filePath) const
{
	return textureDatas_.contains(NormalizePath(filePath));
}

bool TextureManager::CheckTextureExists(const std::string& filePath) const
{
	return ResolveTexturePath(filePath).has_value();
}

std::optional<std::filesystem::path> TextureManager::ResolveTexturePath(const std::string& filePath) const
{
	std::wstring filePathW = KCE::StringUtility::ConvertString(filePath);
	if (std::filesystem::exists(filePathW))
	{
		return std::filesystem::path(filePathW);
	}

	std::filesystem::path resolved = PathManager::ResolveApplicationResource(filePath);
	if (std::filesystem::exists(resolved))
	{
		return resolved;
	}

	std::filesystem::path appRoot = PathManager::GetApplicationResourceRoot();
	std::wstring filename = std::filesystem::path(filePathW).filename().wstring();
	std::vector<std::filesystem::path> searchPaths = {
		appRoot / "textures" / filePathW,
		appRoot / "fonts" / filePathW,
		appRoot / filePathW,
		appRoot / "textures" / filename,
		appRoot / "fonts" / filename
	};

	for (const auto& path : searchPaths)
	{
		if (std::filesystem::exists(path))
		{
			return path;
		}
	}
	return std::nullopt;
}
} // namespace KCE
