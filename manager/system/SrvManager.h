#pragma once
#include "base/DirectXCommon.h"

/**
 * @brief SRV（Shader Resource View）を管理するクラス
 * 
 * DirectX12のシェーダーリソースビュー（SRV）を管理するマネージャークラスです。
 * ディスクリプタヒープの生成・管理、テクスチャやStructured BufferのSRV作成、
 * 描画パイプラインへのバインディングを担当します。
 * 
 * 主な機能:
 * - ディスクリプタヒープの管理（最大512個のSRV）
 * - 2Dテクスチャ、キューブマップテクスチャのSRV作成
 * - Structured Buffer（インスタンシング等）のSRV作成
 * - CPUハンドル（SRV作成時）とGPUハンドル（シェーダーバインド時）の取得
 * 
 * @note SRVはシェーダーからリソースを読み取るためのビューであり、
 *       テクスチャやバッファをシェーダーで使用するために必要です。
 */
class SrvManager
{
public:
	/**
	 * @brief SrvManagerの初期化
	 * @param dxCommon DirectXCommonへのポインタ
	 */
	void Initialize(DirectXCommon* dxCommon);

	/**
	 * @brief 単一のSRVインデックスを確保する
	 * 
	 * ディスクリプタヒープから1つのSRVスロットを確保し、
	 * そのインデックスを返します。
	 * 
	 * @return 確保されたSRVインデックス
	 * @note 最大SRV数に達している場合はアサートで停止します
	 */
	uint32_t Allocate();

	/**
	 * @brief 連続するSRVインデックスを確保する
	 * 
	 * 配列テクスチャやテクスチャアトラスなど、連続したSRVスロットが
	 * 必要な場合に使用します。
	 * 
	 * @param count 確保するSRVの数
	 * @return 確保された連続するSRVの開始インデックス
	 * @note 最大SRV数を超える場合はアサートで停止します
	 */
	uint32_t AllocateRange(uint32_t count);

	/**
	 * @brief 2DテクスチャのSRVを作成する
	 * 
	 * 通常の2Dテクスチャ用のSRVを指定されたインデックスに作成します。
	 * ViewDimensionはD3D12_SRV_DIMENSION_TEXTURE2Dに設定されます。
	 * 
	 * @param srvIndex SRVを作成するインデックス
	 * @param pResource テクスチャリソース
	 * @param format テクスチャのフォーマット（DXGI_FORMAT）
	 * @param mipLevels ミップマップレベル数
	 */
	void CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format,UINT mipLevels);

	/**
	 * @brief キューブマップテクスチャのSRVを作成する
	 * 
	 * 環境マップやスカイボックス用のキューブマップテクスチャのSRVを作成します。
	 * ViewDimensionはD3D12_SRV_DIMENSION_TEXTURECUBEに設定されます。
	 * キューブマップは6面のテクスチャで構成され、360度の環境を表現できます。
	 * 
	 * @param srvIndex SRVを作成するインデックス
	 * @param pResource キューブマップテクスチャリソース
	 * @param format テクスチャのフォーマット（DXGI_FORMAT）
	 * @param mipLevels ミップマップレベル数
	 */
	void CreateSRVforTexture2DCubeMap(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels);

	/**
	 * @brief Structured BufferのSRVを作成する
	 * 
	 * 構造化バッファ用のSRVを作成します。インスタンシング描画や
	 * GPUパーティクルなど、シェーダーで構造体の配列にアクセスする
	 * 場合に使用します。
	 * 
	 * @param srvIndex SRVを作成するインデックス
	 * @param pResource バッファリソース
	 * @param numElements バッファ内の要素数（構造体の数）
	 * @param structureByteStride 1要素あたりのバイトサイズ（sizeof構造体）
	 */
	void CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

	/**
	 * @brief 描画前処理
	 * 
	 * 描画コマンドリストにSRV用のディスクリプタヒープをセットします。
	 * 描画ループの開始時に1度呼び出してください。
	 */
	void PreDraw();

	/**
	 * @brief ルートシグネチャにSRVをバインドする
	 * 
	 * 指定されたルートパラメータインデックスに単一のSRVをバインドします。
	 * シェーダーでテクスチャを使用する前に呼び出してください。
	 * 
	 * @param RootParameterIndex ルートシグネチャのパラメータインデックス
	 * @param srvIndex バインドするSRVのインデックス
	 */
	void SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex);

	/**
	 * @brief ルートシグネチャに連続するSRV範囲をバインドする
	 * 
	 * 指定されたルートパラメータインデックスに連続するSRV範囲の
	 * 開始位置をバインドします。テクスチャ配列やマルチテクスチャを
	 * 使用する場合に使用します。
	 * 
	 * @param RootParameterIndex ルートシグネチャのパラメータインデックス
	 * @param startSrvIndex バインドするSRV範囲の開始インデックス
	 */
	void SetGraphicsRootDescriptorTableRange(UINT RootParameterIndex, uint32_t startSrvIndex);

	/**
	 * @brief 現在使用中のSRVインデックス数を取得する
	 * @return 確保済みSRV数（デバッグ表示用）
	 */
	uint32_t GetUseIndex() const { return useIndex_; }

	/**
	 * @brief 現在アクティブなSRV数を取得する（解放済みを差し引いた実使用数）
	 * @return useIndex_ - freeList_.size()
	 */
	uint32_t GetActiveSRVCount() const
	{
		return useIndex_ - static_cast<uint32_t>(freeList_.size());
	}

	/**
	 * @brief SRV数が最大に達しているか確認する
	 * @return true: 最大数に達している, false: まだ空きがある
	 */
	bool IsMaxSRVCount();


public: // アクセッサ

	/**
	 * @brief SRVディスクリプタヒープを取得する
	 * @return SRVディスクリプタヒープへのポインタ
	 */
	ID3D12DescriptorHeap* GetSrvHeap() { return descriptorHeap_.Get(); }

	/**
	 * @brief CPUディスクリプタハンドルを取得する
	 * 
	 * SRVの作成時に使用するCPUハンドルを取得します。
	 * CPUハンドルはリソースのビュー作成に使用されます。
	 * 
	 * @param index SRVインデックス
	 * @return 指定されたインデックスのCPUディスクリプタハンドル
	 */
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);

	/**
	 * @brief GPUディスクリプタハンドルを取得する
	 * 
	 * シェーダーへのバインド時に使用するGPUハンドルを取得します。
	 * GPUハンドルは描画時のリソース参照に使用されます。
	 * 
	 * @param index SRVインデックス
	 * @return 指定されたインデックスのGPUディスクリプタハンドル
	 */
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

public:
	// 最大SRV数（512個：一般的なゲームで十分な数、GPUメモリ効率のバランス）
	static const uint32_t kMaxSRVCount;
	// 「未確保」を表す番兵値。各クラスのSRVフィールドはこの値で初期化する
	static constexpr uint32_t kInvalidSrvIndex = UINT32_MAX;

private:
	// DirectXCommonへのポインタ
	DirectXCommon* dxCommon_ = nullptr;

	// SRVのディスクリプタサイズ（GPU依存の値）
	uint32_t descriptorSize_;
	// SRVのディスクリプタヒープ（SRVを格納するGPUメモリ領域）
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
	// 次に使用するSRVインデックス（確保済みの数を追跡）
	uint32_t useIndex_ = 0;

	// 解放済みインデックスリスト
	std::vector<uint32_t> freeList_;

public:
	/**
	 * @brief SRVインデックスを解放する
	 * @param index 解放するSRVインデックス
	 */
	void Free(uint32_t index);


};

