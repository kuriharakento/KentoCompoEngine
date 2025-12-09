#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>

#include "base/DirectXCommon.h"
#include "base/GraphicsTypes.h"

class SrvManager;

/**
 * @brief スキニングコンピュートクラス
 * @details コンピュートシェーダーを使用してGPU上でスキニング計算を行う
 */
class SkinningCompute
{
public:
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
	 */
	void Dispatch();

public: // アクセッサ
	/**
	 * @brief 出力バッファのSRVインデックスを取得
	 */
	uint32_t GetOutputSrvIndex() const { return outputSrvIndex_; }

private:
	/**
	 * @brief ルートシグネチャの作成
	 */
	void CreateRootSignature();

	/**
	 * @brief パイプラインステートの作成
	 */
	void CreatePipelineState();

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

	// ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

	// パイプラインステート
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

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

	// ディスクリプタインデックス
	uint32_t boneMatrixSrvIndex_ = 0;
	uint32_t inputSrvIndex_ = 0;
	uint32_t outputSrvIndex_ = 0;
	uint32_t outputUavIndex_ = 0;

	// 現在の頂点数
	uint32_t currentVertexCount_ = 0;

	// 入出力バッファ
	ID3D12Resource* inputBuffer_ = nullptr;
	ID3D12Resource* outputBuffer_ = nullptr;

	// 初回Dispatchフラグ（リソース状態追跡用）
	bool isFirstDispatch_ = true;
};
