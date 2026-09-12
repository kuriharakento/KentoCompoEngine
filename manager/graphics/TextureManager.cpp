#include "TextureManager.h"

#include <algorithm>
#include <filesystem>
#include <iterator>

// system
#include "base/PathManager.h"
#include "base/StringUtility.h"
#include "base/Logger.h"
#include "externals/DirectXTex/d3dx12.h"

namespace KCE
{
namespace
{
const std::string kFallbackTexturePath = "textures/white1x1.png";
}
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

void TextureManager::LoadTexture(const std::string& filePath, ResourceLifetime lifetime)
{
	// パスを正規化（重複読み込み防止用）
	std::string normalizedPath = NormalizePath(filePath);

	/*--------------[ 読み込み済みテクスチャを検索 ]-----------------*/
	if (textureDatas_.contains(normalizedPath) || filePathToIndex_.contains(normalizedPath) || failedTexturePaths_.contains(normalizedPath))
	{
		if (lifetime == ResourceLifetime::Resident)
		{
			MarkResident(normalizedPath);
		}
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
		std::filesystem::path resolved = PathManager::ResolveApplicationResource(filePath);
		if (std::filesystem::exists(resolved))
		{
			targetPath = resolved.wstring();
		}
		else
		{
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
					targetPath = path.wstring();
					break;
				}
			}
		}
	}

	HRESULT hr;
	const auto useFallback = [&]()
	{
		if (failedTexturePaths_.insert(normalizedPath).second)
		{
			Logger::Log("テクスチャを読み込めませんでした: " + filePath + "\n", Logger::LogLevel::Error);
		}
		const std::string fallbackPath = NormalizePath(kFallbackTexturePath);
		// 代替テクスチャ自身の失敗時は再帰せず、未登録のまま戻す。
		if (normalizedPath == fallbackPath)
		{
			// アセット単体の破損ではなく既定リソースが無い状態なので、原因が分かるように別で知らせる
			Logger::Log("エンジンの既定リソース " + kFallbackTexturePath + " を読み込めない。読めなかったテクスチャの代わりが無いので、表示が崩れる\n", Logger::LogLevel::Error);
			return;
		}
		LoadTexture(kFallbackTexturePath, ResourceLifetime::Resident);
		auto fallback = filePathToIndex_.find(fallbackPath);
		if (fallback != filePathToIndex_.end())
		{
			filePathToIndex_[normalizedPath] = fallback->second;
		}
	};

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
	if (FAILED(hr))
	{
		useFallback();
		return;
	}

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
	if (FAILED(hr))
	{
		useFallback();
		return;
	}

	/*--------------[ テクスチャデータを追加 ]-----------------*/

	// 正規化パスをキーにしてテクスチャデータを登録
	TextureData& textureData = textureDatas_[normalizedPath];

	/*--------------[ テクスチャデータの書き込み ]-----------------*/

	textureData.metadata = mipImages.GetMetadata();
	textureData.resource = dxCommon_->CreateTextureResource(textureData.metadata);
	textureData.lifetime = lifetime;
	// 中間リソースをリストに追加して保持（後でまとめて解放）
	intermediateResources_.push_back(UploadTextureData(textureData.resource, mipImages));

	/*--------------[ ディスクリプタハンドルの計算 ]-----------------*/

	// テクスチャデータの要素数番号をSRVのインデックスとする
	uint32_t srvIndex = static_cast<uint32_t>(textureDatas_.size() - 1) + kSRVIndexTop;

	textureData.srvHandleCPU = GetSrvHandleCPU(srvIndex);
	textureData.srvHandleGPU = GetSrvHandleGPU(srvIndex);

	/*--------------[ SRVの生成 ]-----------------*/

	// テクスチャは1枚ずつ参照されるので、返却済みの番号を使い回してよい
	textureData.srvIndex = srvManager_->AllocateReusable();

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

void TextureManager::RegisterTexture(const std::string& key, Microsoft::WRL::ComPtr<ID3D12Resource> resource, const DirectX::TexMetadata& metadata)
{
	const std::string normalizedPath = NormalizePath(key);
	if (!resource || textureDatas_.contains(normalizedPath) || filePathToIndex_.contains(normalizedPath))
	{
		return;
	}
	assert(!srvManager_->IsMaxSRVCount());

	TextureData& textureData = textureDatas_[normalizedPath];
	textureData.metadata = metadata;
	textureData.resource = resource;
	// 外から登録するアトラスなどは、登録元と寿命を合わせて常駐させる。
	textureData.lifetime = ResourceLifetime::Resident;

	textureData.srvIndex = srvManager_->AllocateReusable();
	srvManager_->CreateSRVforTexture2D(textureData.srvIndex, textureData.resource.Get(), textureData.metadata.format, static_cast<UINT>(textureData.metadata.mipLevels));
	textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
	textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);

	filePathToIndex_[normalizedPath] = textureData.srvIndex;
	indexToFilePath_[textureData.srvIndex] = normalizedPath;
}

void TextureManager::ClearIntermediateResources()
{
	// 中間リソースを一括解放
	intermediateResources_.clear();
}

void TextureManager::MarkResident(const std::string& filePath)
{
	auto it = textureDatas_.find(NormalizePath(filePath));
	if (it != textureDatas_.end())
	{
		it->second.lifetime = ResourceLifetime::Resident;
	}
}

void TextureManager::ReleaseSceneResources()
{
	for (auto textureIt = textureDatas_.begin(); textureIt != textureDatas_.end();)
	{
		if (textureIt->second.lifetime == ResourceLifetime::Resident)
		{
			++textureIt;
			continue;
		}

		const uint32_t srvIndex = textureIt->second.srvIndex;
		for (auto pathIt = filePathToIndex_.begin(); pathIt != filePathToIndex_.end();)
		{
			pathIt = pathIt->second == srvIndex ? filePathToIndex_.erase(pathIt) : std::next(pathIt);
		}
		indexToFilePath_.erase(srvIndex);
		srvManager_->Free(srvIndex);
		textureIt = textureDatas_.erase(textureIt);
	}

	// GPU待ちの直後なので、同じタイミングで転送用バッファも手放せる。
	ClearIntermediateResources();
}

size_t TextureManager::GetResidentTextureCount() const
{
	return static_cast<size_t>(std::count_if(textureDatas_.begin(), textureDatas_.end(), [](const auto& item)
	{
		return item.second.lifetime == ResourceLifetime::Resident;
	}));
}

size_t TextureManager::GetSceneTextureCount() const
{
	return textureDatas_.size() - GetResidentTextureCount();
}

uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath)
{
	// パスを正規化
	std::string normalizedPath = NormalizePath(filePath);
	// ファイルパスが登録されているか確認
	auto found = filePathToIndex_.find(normalizedPath);
	if (found != filePathToIndex_.end())
	{
		return found->second;
	}

	if (failedTexturePaths_.insert(normalizedPath).second)
	{
		Logger::Log("未登録のテクスチャが指定されました: " + filePath + "\n", Logger::LogLevel::Error);
	}
	LoadTexture(kFallbackTexturePath, ResourceLifetime::Resident);
	const auto fallback = filePathToIndex_.find(NormalizePath(kFallbackTexturePath));
	if (fallback != filePathToIndex_.end())
	{
		// 次回以降はログも再検索もせず同じ代替テクスチャを返す。
		filePathToIndex_[normalizedPath] = fallback->second;
		return fallback->second;
	}

	// 代替も無いときは index 0（ImGui のフォント）を返すしかない。落とさない代わりに、何が起きているかを1回だけ出しておく
	if (failedTexturePaths_.insert("<fallback-missing>").second)
	{
		Logger::Log("代替テクスチャ " + kFallbackTexturePath + " が無いため index 0 を返す。既定リソースが揃っているか確認して\n", Logger::LogLevel::Error);
	}
	return 0;
}

const DirectX::TexMetadata& TextureManager::GetMetadata(uint32_t textureIndex)
{
	// インデックスがマッピング内に存在するか確認
	assert(indexToFilePath_.contains(textureIndex));
	const std::string& filePath = indexToFilePath_[textureIndex];
	return textureDatas_.at(filePath).metadata;
}

const DirectX::TexMetadata& TextureManager::GetMetadata(const std::string& filePath)
{
	return GetMetadata(GetTextureIndexByFilePath(filePath));
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
} // namespace KCE
