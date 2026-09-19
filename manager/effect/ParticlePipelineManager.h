#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <unordered_map>
#include <string>
#include <dxcapi.h>

#include "math/BlendMode.h"

namespace KCE
{
class DirectXCommon;

/**
 * @brief パーティクル用パイプラインマネージャークラス
 * @details ブレンドモード別のパイプラインステートを管理する
 *          Alpha、Additive、Multiply等の各ブレンドモードに対応した
 *          パイプラインステートを生成・保持する
 */
class ParticlePipelineManager
{
public:
    ParticlePipelineManager() = default;  // コンストラクタ
    ~ParticlePipelineManager() = default; // デストラクタ

    /**
     * @brief 初期化処理
     * @param dxCommon DirectXCommonへのポインタ
     * @details ルートシグネチャと各ブレンドモードのパイプラインステートを生成する
     */
    void Initialize(DirectXCommon* dxCommon);

    /**
     * @brief パイプラインステートの取得
     * @param mode 取得したいブレンドモード
     * @return 指定されたブレンドモードのパイプラインステート
     */
	ID3D12PipelineState* GetPipelineState(BlendMode mode, bool bloomEnabled = false) const;
	void SetSelectiveBloomOutputEnabled(bool enabled) { selectiveBloomOutputEnabled_ = enabled; }

    /**
     * @brief ルートシグネチャの取得
     * @return ルートシグネチャへのポインタ
     */
    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }

    //===== リボン用パイプライン =====//

    /**
     * @brief リボン用パイプラインステートの取得
     * @param mode ブレンドモード
     * @return リボン用パイプラインステート
     */
    ID3D12PipelineState* GetRibbonPipelineState(BlendMode mode, bool bloomEnabled = false) const;

    /**
     * @brief リボン用ルートシグネチャの取得
     * @return リボン用ルートシグネチャ
     */
    ID3D12RootSignature* GetRibbonRootSignature() const { return ribbonRootSignature_.Get(); }
	double GetPrewarmMilliseconds() const { return prewarmMilliseconds_; }
	uint32_t GetPsoCreationCount() const { return psoCreationCount_; }

private:
    /**
     * @brief ルートシグネチャの生成
     */
    void CreateRootSignature();

    /**
     * @brief グラフィックスパイプラインステートの生成
     * @param mode 生成するブレンドモード
     */
	void CreateGraphicsPipelineState(BlendMode mode, bool bloomEnabled, bool bloomTargetEnabled);

    /**
     * @brief リボン用ルートシグネチャの生成
     */
    void CreateRibbonRootSignature();

    /**
     * @brief リボン用パイプラインステートの生成
     * @param mode ブレンドモード
     */
	void CreateRibbonPipelineState(BlendMode mode, bool bloomEnabled, bool bloomTargetEnabled);

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_; // ルートシグネチャ
    std::unordered_map<BlendMode, Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelines_; // ブレンドモード別パイプラインステート
    std::unordered_map<BlendMode, Microsoft::WRL::ComPtr<ID3D12PipelineState>> bloomPipelines_;
	std::unordered_map<BlendMode, Microsoft::WRL::ComPtr<ID3D12PipelineState>> singleTargetPipelines_;

    // リボン用パイプライン
    Microsoft::WRL::ComPtr<ID3D12RootSignature> ribbonRootSignature_;
    std::unordered_map<BlendMode, Microsoft::WRL::ComPtr<ID3D12PipelineState>> ribbonPipelines_;
    std::unordered_map<BlendMode, Microsoft::WRL::ComPtr<ID3D12PipelineState>> ribbonBloomPipelines_;
	std::unordered_map<BlendMode, Microsoft::WRL::ComPtr<ID3D12PipelineState>> ribbonSingleTargetPipelines_;

	DirectXCommon* dxCommon_ = nullptr; // DirectXCommonへのポインタ
	Microsoft::WRL::ComPtr<IDxcBlob> particleVertexShader_;
	Microsoft::WRL::ComPtr<IDxcBlob> particlePixelShader_;
	Microsoft::WRL::ComPtr<IDxcBlob> particleSingleTargetPixelShader_;
	Microsoft::WRL::ComPtr<IDxcBlob> ribbonVertexShader_;
	Microsoft::WRL::ComPtr<IDxcBlob> ribbonPixelShader_;
	Microsoft::WRL::ComPtr<IDxcBlob> ribbonSingleTargetPixelShader_;
	double prewarmMilliseconds_ = 0.0;
	bool selectiveBloomOutputEnabled_ = true;
	uint32_t psoCreationCount_ = 0;
};
} // namespace KCE
