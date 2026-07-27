#include "TextureManager.h"

#include <algorithm>
#include <filesystem>

// system
#include "base/StringUtility.h"
#include "externals/DirectXTex/d3dx12.h"

// SRVインデックスの開始番号の実体（ImGuiが0番を使用するため、1番から開始）
uint32_t TextureManager::kSRVIndexTop = 1;

// シングルトンインスタンスの実体
std::unique_ptr<TextureManager> TextureManager::instance_ = nullptr;

TextureManager* TextureManager::GetInstance()
{
	// インスタンスが存在しない場合は生成
	if (instance_ == nullptr)
	{
		instance_.reset(new TextureManager());
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
	// パスを正規化（重複読み込み防止用）
	std::string normalizedPath = NormalizePath(filePath);

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
	// ファイル読み込みには元のパスを使用
	std::wstring filePathW = KCE::StringUtility::ConvertString(filePath);

	// ファイルが存在しない場合の自動検索処理（相対パスやファイル名のみに対応）
	std::wstring targetPath = filePathW;
	if (!std::filesystem::exists(targetPath))
	{
		std::wstring filename = std::filesystem::path(filePathW).filename().wstring();
		std::vector<std::wstring> searchPaths = {
			// 1. サブディレクトリ階層を考慮した検索
			L"application/Resources/textures/" + filePathW,
			L"application/Resources/fonts/" + filePathW,
			L"application/Resources/" + filePathW,
			L"Resources/textures/" + filePathW,
			L"Resources/fonts/" + filePathW,
			L"Resources/" + filePathW,
			L"../engine/Resources/textures/" + filePathW,
			L"../engine/Resources/fonts/" + filePathW,
			L"../engine/Resources/" + filePathW,

			// 2. ファイル名のみのフォールバック検索
			L"application/Resources/textures/" + filename,
			L"application/Resources/fonts/" + filename,
			L"Resources/textures/" + filename,
			L"Resources/fonts/" + filename,
			L"../engine/Resources/textures/" + filename,
			L"../engine/Resources/fonts/" + filename
		};

		for (const auto& path : searchPaths)
		{
			if (std::filesystem::exists(path))
			{
				targetPath = path;
				break;
			}
		}
	}

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
		// 圧縮フォーマットならそのまま使用
		mipImages = std::move(image);
	}
	else
	{
		// 非圧縮フォーマットの場合はミップマップを生成
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

	// 正規化パスをキーにしてテクスチャデータを登録
	TextureData& textureData = textureDatas_[normalizedPath];

	/*--------------[ テクスチャデータの書き込み ]-----------------*/

	textureData.metadata = mipImages.GetMetadata();
	textureData.resource = dxCommon_->CreateTextureResource(textureData.metadata);
	// 中間リソースをリストに追加して保持（後でまとめて解放）
	intermediateResources_.push_back(UploadTextureData(textureData.resource, mipImages));

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
	// パスを正規化
	std::string normalizedPath = NormalizePath(filePath);
	// ファイルパスが登録されているか確認
	assert(filePathToIndex_.contains(normalizedPath));
	return filePathToIndex_[normalizedPath];
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
