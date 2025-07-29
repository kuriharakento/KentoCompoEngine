#pragma once
#include <d3d12.h>
#include <string>
#include <wrl.h>
#include <unordered_map>

// system
#include "base/DirectXCommon.h"
#include "manager/system/SrvManager.h"

/**
 * \brief テクスチャマネージャー
 */
class TextureManager
{
public:
	/// \brief インスタンス取得
	static TextureManager* GetInstance();
	/// \brief インスタンス解放
	void Finalize();
	/// \brief 初期化
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	/// \brief テクスチャの読み込み
	void LoadTexture(const std::string& filePath);

public: //アクセッサ
	/// \brief SRVインデックスの開始番号
	uint32_t GetTextureIndexByFilePath(const std::string& filePath);
	//メタデータの取得
	const DirectX::TexMetadata& GetMetadata(uint32_t textureIndex);
	const DirectX::TexMetadata& GetMetadata(const std::string& filePath) { return textureDatas_[filePath].metadata; }
	//SRVインデックスの取得
	uint32_t GetSRVIndex(const std::string& filePath) { return textureDatas_[filePath].srvIndex; }
	//GPUハンドルの取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath) { return textureDatas_[filePath].srvHandleGPU; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(uint32_t textureIndex) { return textureDatas_[indexToFilePath_[textureIndex]].srvHandleGPU; }
	//CPUハンドルの取得
	D3D12_CPU_DESCRIPTOR_HANDLE GetSrvHandleCPU(const std::string& filePath) { return textureDatas_[filePath].srvHandleCPU; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetSrvHandleCPU(uint32_t textureIndex) { return textureDatas_[indexToFilePath_[textureIndex]].srvHandleCPU; }

private: //構造体
	/// \brief テクスチャデータ
	struct TextureData
	{
		DirectX::TexMetadata metadata;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		Microsoft::WRL::ComPtr<ID3D12Resource> intermediate;
		uint32_t srvIndex;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
	};

	//テクスチャデータ
	std::unordered_map<std::string, TextureData> textureDatas_;
	// ファイルパスからインデックスを取得するマップ
	std::unordered_map<std::string, uint32_t> filePathToIndex_;
	// インデックスからファイルパスを取得するマップ
	std::unordered_map<uint32_t, std::string> indexToFilePath_;

private: //メンバ関数
	
	/**
	 * \brief 
	 * \param texture テクスチャリソースの転送
	 * \param mipImages 
	 * \return 
	 */
	[[nodiscard]]
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource> texture, const DirectX::ScratchImage& mipImages);

private: //メンバ変数
	//DirectXコマンド
	DirectXCommon* dxCommon_ = nullptr;

	//SRVマネージャー
	SrvManager* srvManager_ = nullptr;

	//SRVインデックスの開始番号
	static uint32_t kSRVIndexTop;

private: //シングルトンインスタンス
	static TextureManager* instance_;

	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(TextureManager&) = delete;
	TextureManager& operator=(TextureManager&) = delete;



};

