#pragma once
#include <d3d12.h>
#include <wrl.h>

#include "base/DirectXCommon.h"

namespace KCE
{
/**
 * @brief スプライト共通部クラス
 * @details スプライト描画に必要なルートシグネチャとパイプラインステートを管理する
 */
class SpriteCommon
{
public: // メンバ関数
	/**
	 * @brief 初期化
	 * @param dxCommon DirectXCommonへのポインタ
	 */
	void Initialize(DirectXCommon* dxCommon);

	/**
	 * @brief 共通描画設定
	 * @details ルートシグネチャ、パイプラインステート、プリミティブトポロジーを設定する
	 */
	void CommonRenderingSetting(bool bloomEnabled = false);

public: // アクセッサ
	/**
	 * @brief DirectXCommonの取得
	 * @return DirectXCommonへのポインタ
	 */
	DirectXCommon* GetDXCommon() const { return dxCommon_; }

private: // メンバ関数
	/**
	 * @brief ルートシグネチャの生成
	 * @details スプライト用のルートシグネチャを構築する
	 */
	void CreateRootSignature();

	/**
	 * @brief グラフィックスパイプラインステートの生成
	 * @details スプライト用のパイプラインステートを構築する
	 */
	void CreateGraphicsPipelineState();

private: // メンバ変数
	// DirectXコマンド
	DirectXCommon *dxCommon_ = nullptr;

	// ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
	// グラフィックスパイプラインステート
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> bloomGraphicsPipelineState_ = nullptr;


};
} // namespace KCE
