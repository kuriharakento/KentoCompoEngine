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
	void CommonRenderingSetting();

	/**
	 * @brief 3D のシーン（カラーは HDR 形式、深度あり・書き込みなし）へ描く版の共通描画設定
	 * @details 画面の 2D 用とはレンダーターゲットの形式が違うので、パイプラインだけ別。3D のゲージが使う。
	 *          描き終えたあと、2D に戻すなら CommonRenderingSetting を呼び直す
	 * @param depthTest 真なら奥の物に隠れる。偽なら深度を見ず、常に手前に描く
	 */
	void SceneRenderingSetting(bool depthTest = true);

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
	// 3D のシーンへ描く版
	Microsoft::WRL::ComPtr<ID3D12PipelineState> scenePipelineState_ = nullptr;
	// 3D のシーンへ描く版で、深度を見ない
	Microsoft::WRL::ComPtr<ID3D12PipelineState> sceneNoDepthPipelineState_ = nullptr;


};
} // namespace KCE
