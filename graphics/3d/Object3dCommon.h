#pragma once
#include "base/DirectXCommon.h"
#include "base/Camera.h"

namespace KCE
{
class SrvManager;
class LightManager;

/**
 * @brief 3Dオブジェクト共通部クラス
 * @details 3Dオブジェクト描画に必要なルートシグネチャとパイプラインステートを管理する
 */
class Object3dCommon
{
public: // メンバ関数
	/**
	 * @brief 初期化
	 * @param dxCommon DirectXCommonへのポインタ
	 * @param srvManager SRVマネージャーへのポインタ
	 */
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	/**
	 * @brief 共通描画設定
	 * @details ルートシグネチャ、パイプラインステート、環境マップを設定する
	 */
	void CommonRenderingSetting();

public: // アクセッサ
	/**
	 * @brief DirectXCommonの取得
	 * @return DirectXCommonへのポインタ
	 */
	DirectXCommon* GetDXCommon() const { return dxCommon_; }

	/**
	 * @brief デフォルトカメラの設定
	 * @param camera カメラへのポインタ
	 */
	void SetDefaultCamera(Camera* camera) { defaultCamera_ = camera; }

	/**
	 * @brief デフォルトカメラの取得
	 * @return デフォルトカメラへのポインタ
	 */
	Camera* GetDefaultCamera() const { return defaultCamera_; }

	/**
	 * @brief SRVマネージャーの取得
	 * @return SRVマネージャーへのポインタ
	 */
	SrvManager* GetSrvManager() const { return srvManager_; }

	/**
	 * @brief デフォルトライトマネージャの設定
	 * @param lightManager ライトマネージャへのポインタ
	 */
	void SetDefaultLightManager(LightManager* lightManager) { defaultLightManager_ = lightManager; }

	/**
	 * @brief デフォルトライトマネージャの取得
	 * @return デフォルトライトマネージャへのポインタ
	 */
	LightManager* GetDefaultLightManager() const { return defaultLightManager_; }

private: // メンバ関数
	/**
	 * @brief ルートシグネチャの生成
	 * @details 3Dオブジェクト用のルートシグネチャを構築する
	 */
	void CreateRootSignature();

	/**
	 * @brief グラフィックスパイプラインステートの生成
	 * @details 3Dオブジェクト用のパイプラインステートを構築する
	 */
	void CreateGraphicsPipelineState();

private: // メンバ変数
	// デフォルトカメラ
	Camera* defaultCamera_ = nullptr;

	// DirectXCommonへのポインタ
	DirectXCommon* dxCommon_ = nullptr;

	// SRVマネージャーへのポインタ
	SrvManager* srvManager_ = nullptr;

	// デフォルトライトマネージャ
	LightManager* defaultLightManager_ = nullptr;

	// ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
	// グラフィックスパイプラインステート
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_ = nullptr;

};
} // namespace KCE
