#pragma once
#include <d3d12.h>
#include <string>
#include <wrl.h>
#include <unordered_map>
#include <vector>

// system
#include "base/DirectXCommon.h"
#include "manager/system/SrvManager.h"

/**
 * @brief テクスチャマネージャークラス
 * @details テクスチャのロード、キャッシング、SRVインデックス管理を行うシングルトンクラス
 *          DDS、WIC形式のテクスチャに対応し、ミップマップ生成も行う
 */
class TextureManager
{
public:
	/**
	 * @brief シングルトンインスタンスを取得
	 * @return TextureManagerのインスタンス
	 */
	static TextureManager* GetInstance();

	/**
	 * @brief 終了処理
	 * @details インスタンスを解放する
	 */
	void Finalize();

	/**
	 * @brief 初期化処理
	 * @param dxCommon DirectXCommonへのポインタ
	 * @param srvManager SRVマネージャーへのポインタ
	 */
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	/**
	 * @brief テクスチャの読み込み
	 * @param filePath テクスチャファイルパス
	 * @details 読み込み済みの場合はスキップされる
	 */
	void LoadTexture(const std::string& filePath);

public: // アクセッサ
	/**
	 * @brief ファイルパスからテクスチャインデックスを取得
	 * @param filePath テクスチャファイルパス
	 * @return テクスチャインデックス
	 */
	uint32_t GetTextureIndexByFilePath(const std::string& filePath);

	/**
	 * @brief インデックスからメタデータを取得
	 * @param textureIndex テクスチャインデックス
	 * @return テクスチャのメタデータ
	 */
	const DirectX::TexMetadata& GetMetadata(uint32_t textureIndex);

	/**
	 * @brief ファイルパスからメタデータを取得
	 * @param filePath テクスチャファイルパス
	 * @return テクスチャのメタデータ
	 */
	const DirectX::TexMetadata& GetMetadata(const std::string& filePath) { return textureDatas_[filePath].metadata; }

	/**
	 * @brief SRVインデックスを取得
	 * @param filePath テクスチャファイルパス
	 * @return SRVインデックス
	 */
	uint32_t GetSRVIndex(const std::string& filePath) { return textureDatas_[filePath].srvIndex; }

	/**
	 * @brief GPU側のディスクリプタハンドルを取得
	 * @param filePath テクスチャファイルパス
	 * @return GPUディスクリプタハンドル
	 */
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath) { return textureDatas_[filePath].srvHandleGPU; }

	/**
	 * @brief インデックスからGPU側のディスクリプタハンドルを取得
	 * @param textureIndex テクスチャインデックス
	 * @return GPUディスクリプタハンドル
	 */
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(uint32_t textureIndex) { return textureDatas_[indexToFilePath_[textureIndex]].srvHandleGPU; }

	/**
	 * @brief CPU側のディスクリプタハンドルを取得
	 * @param filePath テクスチャファイルパス
	 * @return CPUディスクリプタハンドル
	 */
	D3D12_CPU_DESCRIPTOR_HANDLE GetSrvHandleCPU(const std::string& filePath) { return textureDatas_[filePath].srvHandleCPU; }

	/**
	 * @brief インデックスからCPU側のディスクリプタハンドルを取得
	 * @param textureIndex テクスチャインデックス
	 * @return CPUディスクリプタハンドル
	 */
	D3D12_CPU_DESCRIPTOR_HANDLE GetSrvHandleCPU(uint32_t textureIndex) { return textureDatas_[indexToFilePath_[textureIndex]].srvHandleCPU; }

	/**
	 * @brief 読み込み済みテクスチャのパス一覧を取得
	 * @return テクスチャパスのベクター
	 */
	std::vector<std::string> GetLoadedTexturePaths() const
	{
		std::vector<std::string> paths;
		paths.reserve(textureDatas_.size());
		for (const auto& pair : textureDatas_)
		{
			paths.push_back(pair.first);
		}
		return paths;
	}

private: // 構造体
	/**
	 * @brief テクスチャデータ
	 */
	struct TextureData
	{
		DirectX::TexMetadata metadata;                        // テクスチャのメタデータ
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;      // テクスチャリソース
		Microsoft::WRL::ComPtr<ID3D12Resource> intermediate;  // 中間バッファ
		uint32_t srvIndex;                                    // SRVインデックス
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;            // CPU側ディスクリプタハンドル
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;            // GPU側ディスクリプタハンドル
	};

	// テクスチャデータのキャッシュ（ファイルパス -> テクスチャデータ）
	std::unordered_map<std::string, TextureData> textureDatas_;
	// ファイルパスからインデックスを取得するマップ
	std::unordered_map<std::string, uint32_t> filePathToIndex_;
	// インデックスからファイルパスを取得するマップ
	std::unordered_map<uint32_t, std::string> indexToFilePath_;

private: // メンバ関数
	
	/**
	 * @brief テクスチャリソースの転送
	 * @param texture テクスチャリソース
	 * @param mipImages ミップマップイメージ
	 * @return 中間バッファ
	 */
	[[nodiscard]]
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource> texture, const DirectX::ScratchImage& mipImages);

private: // メンバ変数
	// DirectXCommonへのポインタ
	DirectXCommon* dxCommon_ = nullptr;

	// SRVマネージャーへのポインタ
	SrvManager* srvManager_ = nullptr;

	// SRVインデックスの開始番号（ImGuiが0番を使用するため1から開始）
	static uint32_t kSRVIndexTop;

private: // シングルトンインスタンス
	static TextureManager* instance_; // シングルトンインスタンス

	TextureManager() = default;                            // コンストラクタ
	~TextureManager() = default;                           // デストラクタ
	TextureManager(TextureManager&) = delete;              // コピーコンストラクタ
	TextureManager& operator=(TextureManager&) = delete;   // 代入演算子



};

