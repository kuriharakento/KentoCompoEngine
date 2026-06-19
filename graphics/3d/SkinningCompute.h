#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>

#include "base/DirectXCommon.h"
#include "base/GraphicsTypes.h"
#include "manager/system/SrvManager.h"

class SrvManager;

/**
 * @brief スキニングコンピュートクラス
 * @details コンピュートシェーダーを使用してGPU上でスキニング計算を行う
 */
class SkinningCompute
{
public:
	/**
	 * @brief デストラクタ。確保したSRV/UAVをFreeする
	 */
	~SkinningCompute();

	/**
	 * @brief 初期化
	 * @param dxCommon DirectXCommonへのポインタ
	 * @param srvManager SRVマネージャーへのポインタ
	 */
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	/**
	 * @brief スキニング計算用リソースの準備
	 * @param vertexCount 頂点数
	 * @param inputBuffer 入力頂点バッファ（SkinnedVertexData）
	 * @param outputBuffer 出力頂点バッファ（VertexData）
	 */
	void PrepareResources(uint32_t vertexCount, ID3D12Resource* inputBuffer, ID3D12Resource* outputBuffer);

	/**
	 * @brief ボーン行列バッファの更新
	 * @param boneMatrices ボーン行列配列
	 */
	void UpdateBoneMatrices(const std::vector<Matrix4x4>& boneMatrices);

	/**
	 * @brief スキニング計算の実行
	 * @param currentState 現在のリソース状態（バリア用）
	 */
	void Dispatch(D3D12_RESOURCE_STATES currentState);

public: // アクセッサ
	/**
	 * @brief 出力バッファのSRVインデックスを取得
	 */
	uint32_t GetOutputSrvIndex() const { return outputSrvIndex_; }

private:
	/**
	 * @brief ボーン行列バッファの作成
	 */
	void CreateBoneMatrixBuffer();

	/**
	 * @brief 定数バッファの作成
	 */
	void CreateConstantBuffer();

private:
	// DirectXCommonへのポインタ
	DirectXCommon* dxCommon_ = nullptr;

	// SRVマネージャーへのポインタ
	SrvManager* srvManager_ = nullptr;

	// ボーン行列バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> boneMatrixBuffer_;
	Matrix4x4* boneMatrixData_ = nullptr;

	// 定数バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	struct SkinningConstants
	{
		uint32_t vertexCount;
		uint32_t padding[3];
	};
	SkinningConstants* constantData_ = nullptr;

	// ディスクリプタインデックス（未確保=kInvalidSrvIndex）
	uint32_t boneMatrixSrvIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t inputSrvIndex_      = SrvManager::kInvalidSrvIndex;
	uint32_t outputSrvIndex_     = SrvManager::kInvalidSrvIndex;
	uint32_t outputUavIndex_     = SrvManager::kInvalidSrvIndex;

	// 現在の頂点数
	uint32_t currentVertexCount_ = 0;

	// 入出力バッファ
	ID3D12Resource* inputBuffer_ = nullptr;
	ID3D12Resource* outputBuffer_ = nullptr;
};
