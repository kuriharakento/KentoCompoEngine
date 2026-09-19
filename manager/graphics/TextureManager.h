#pragma once
#include <d3d12.h>
#include <filesystem>
#include <optional>
#include <string>
#include <wrl.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>

// system
#include "base/DirectXCommon.h"
#include "manager/system/SrvManager.h"
#include "manager/graphics/ResourceLifetime.h"

namespace KCE
{
class JobSystem;

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
	void LoadTexture(const std::string& filePath, ResourceLifetime lifetime = ResourceLifetime::Scene);

	/**
	 * @brief テクスチャをリニア色空間で読み込む
	 * @param filePath テクスチャファイルパス
	 * @param lifetime 解放タイミング
	 * @details 発光マスクのように色ではなく数値として使うテクスチャ向け。
	 *          sRGB版とは別枠で持つので、同じパスを両方で読んでも衝突しない。
	 */
	void LoadTextureLinear(const std::string& filePath, ResourceLifetime lifetime = ResourceLifetime::Scene);

	/**
	 * @brief テクスチャをまとめて読む。ファイルの展開とミップマップ作りだけワーカーで並べて回す
	 * @details GPU のリソース・転送・SRV は並べた順にメインスレッドで作るので、
	 *          SRV の番号の並びは LoadTexture を順に呼んだときと同じ。メインスレッドから呼ぶこと。
	 * @param filePaths 読むテクスチャのパス。読み込み済みと重複は飛ばす
	 * @param lifetime 寿命
	 * @param jobSystem ワーカー。nullptr なら順番に読む（所有しない）
	 */
	void LoadTextures(const std::vector<std::string>& filePaths, ResourceLifetime lifetime, JobSystem* jobSystem);

	/** @brief 読み込み済みテクスチャを常駐扱いへ昇格する */
	void MarkResident(const std::string& filePath);

	/** @brief シーン寿命のテクスチャと転送用中間リソースを解放する */
	void ReleaseSceneResources();

	/**
	 * @brief 外で作ったテクスチャを登録する（実行時に書き換える文字アトラスなど）
	 * @details SRV を作って名前で引けるようにするだけ。中身の転送とリソースの状態は呼ぶ側が持つ。
	 *          同じ名前が登録済みなら何もしない。
	 * @param key 登録名。以降は LoadTexture で読んだものと同じようにこの名前で引ける
	 * @param resource テクスチャ。TextureManager も参照を持つ
	 * @param metadata 大きさとフォーマット。Sprite の UV の計算に使われる
	 */
	void RegisterTexture(const std::string& key, Microsoft::WRL::ComPtr<ID3D12Resource> resource, const DirectX::TexMetadata& metadata);

	/**
	 * @brief 中間リソースを解放する
	 * @details GPUへの転送完了を待機した後に呼び出すこと
	 */
	void ClearIntermediateResources();

	/** @brief 常駐テクスチャ数を取得する */
	size_t GetResidentTextureCount() const;
	/** @brief シーン寿命のテクスチャ数を取得する */
	size_t GetSceneTextureCount() const;

public: // アクセッサ
	/**
	 * @brief ファイルパスからテクスチャインデックスを取得
	 * @param filePath テクスチャファイルパス
	 * @return テクスチャインデックス
	 */
	uint32_t GetTextureIndexByFilePath(const std::string& filePath);

	/**
	 * @brief ファイルパスからリニア版テクスチャのインデックスを取得
	 * @param filePath テクスチャファイルパス
	 * @return テクスチャインデックス。未読み込みならフォールバックのインデックス
	 */
	uint32_t GetLinearTextureIndexByFilePath(const std::string& filePath);

	/**
	 * @brief 読み込み済みなら SRV インデックスを取り出す
	 * @param filePath テクスチャファイルパス
	 * @param outIndex 見つかったインデックス。見つからなければ kInvalidSrvIndex
	 * @return 読み込み済みで SRV が生きていれば true
	 * @note GetTextureIndexByFilePath と違い、フォールバックに逃がさず失敗を返す。
	 */
	bool TryGetTextureIndexByFilePath(const std::string& filePath, uint32_t& outIndex) const;

	/**
	 * @brief そのパスのテクスチャがファイルとして存在するか調べる
	 * @param filePath テクスチャファイルパス
	 * @return 見つかれば true。読み込みはしない
	 */
	bool CheckTextureExists(const std::string& filePath) const;

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
	const DirectX::TexMetadata& GetMetadata(const std::string& filePath);

	/**
	 * @brief SRVインデックスを取得
	 * @param filePath テクスチャファイルパス
	 * @return SRVインデックス
	 */
	uint32_t GetSRVIndex(const std::string& filePath) { return GetTextureIndexByFilePath(filePath); }

	/**
	 * @brief GPU側のディスクリプタハンドルを取得
	 * @param filePath テクスチャファイルパス
	 * @return GPUディスクリプタハンドル
	 */
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath) { return GetSrvHandleGPU(GetTextureIndexByFilePath(filePath)); }

	/**
	 * @brief インデックスからGPU側のディスクリプタハンドルを取得
	 * @param textureIndex テクスチャインデックス（SRV の番号と同じ）
	 * @return GPUディスクリプタハンドル
	 * @details 描くたびに呼ばれるので、文字列のマップを引かずに SRV の番号から直接計算する
	 */
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(uint32_t textureIndex) { return srvManager_->GetGPUDescriptorHandle(textureIndex); }

	/**
	 * @brief CPU側のディスクリプタハンドルを取得
	 * @param filePath テクスチャファイルパス
	 * @return CPUディスクリプタハンドル
	 */
	D3D12_CPU_DESCRIPTOR_HANDLE GetSrvHandleCPU(const std::string& filePath) { return GetSrvHandleCPU(GetTextureIndexByFilePath(filePath)); }


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
		uint32_t srvIndex;                                    // SRVインデックス
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;            // CPU側ディスクリプタハンドル
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;            // GPU側ディスクリプタハンドル
		ResourceLifetime lifetime = ResourceLifetime::Scene;
	};

	// テクスチャデータのキャッシュ（ファイルパス -> テクスチャデータ）
	std::unordered_map<std::string, TextureData> textureDatas_;
	// ファイルパスからインデックスを取得するマップ
	std::unordered_map<std::string, uint32_t> filePathToIndex_;
	// インデックスからファイルパスを取得するマップ
	std::unordered_map<uint32_t, std::string> indexToFilePath_;
	// 同じ壊れたパスを毎フレーム再試行してログを埋めないために記録する。
	std::unordered_set<std::string> failedTexturePaths_;

	// ロード中の中間リソース（GPU転送完了後に ClearIntermediateResources で解放する）
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> intermediateResources_;

private: // メンバ関数
	
	/**
	 * @brief パスを正規化する
	 * @param filePath 正規化対象のファイルパス
	 * @return 正規化されたパス（小文字、スラッシュ区切り）
	 * @details 相対パスを整理し、スラッシュ区切り・小文字に統一する
	 */
	std::string NormalizePath(const std::string& filePath) const;

	/**
	 * @brief 読み込み済みか、前に失敗したパスかを調べる
	 * @details 読み込み済みのものを常駐で読もうとしたら、常駐へ昇格もする
	 * @return 読まなくていいなら true
	 */
	bool IsAlreadyHandled(const std::string& normalizedPath, ResourceLifetime lifetime);

	/**
	 * @brief ファイルを探して展開し、ミップマップまで作る
	 * @details メンバーを書き換えないので、ワーカーから同時に呼んでいい
	 * @param filePath テクスチャファイルパス
	 * @param mipImages 展開結果の書き込み先
	 * @return 失敗したら FAILED な値
	 */
	HRESULT DecodeTexture(const std::string& filePath, DirectX::ScratchImage& mipImages, bool forceSrgb = true) const;

	/**
	 * @brief テクスチャの実ファイルを探す
	 * @param filePath 指定パス。相対でもファイル名だけでもよい
	 * @return 見つかったパス。無ければ空
	 */
	std::optional<std::filesystem::path> ResolveTexturePath(const std::string& filePath) const;

	/**
	 * @brief 展開済みの画像から GPU のリソース・転送・SRV を作って登録する。メインスレッド専用
	 */
	void CommitTexture(const std::string& normalizedPath, const DirectX::ScratchImage& mipImages, ResourceLifetime lifetime);

	/**
	 * @brief 読めなかったテクスチャを記録して、代わりの白いテクスチャを割り当てる
	 */
	void HandleLoadFailure(const std::string& filePath, const std::string& normalizedPath);

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
	static std::unique_ptr<TextureManager> instance_; // シングルトンインスタンス
	friend std::unique_ptr<TextureManager> std::make_unique<TextureManager>();

	TextureManager() = default;                            // コンストラクタ
	TextureManager(const TextureManager&) = delete;       // コピー禁止
	TextureManager& operator=(const TextureManager&) = delete; // 代入禁止

public:
	~TextureManager() = default;                           // デストラクタ
};
} // namespace KCE
