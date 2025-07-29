#pragma once
#include "base/DirectXCommon.h"
#include "base/Camera.h"

class SrvManager;

class Object3dCommon
{
public: //メンバ関数
	/// \brief 初期化
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	//共通描画設定
	void CommonRenderingSetting();

public: //アクセッサ
	DirectXCommon* GetDXCommon() const { return dxCommon_; }

	void SetDefaultCamera(Camera* camera) { defaultCamera_ = camera; }
	Camera* GetDefaultCamera() const { return defaultCamera_; }

private: //メンバ関数
	/// \brief ルートシグネチャの生成
	void CreateRootSignature();
	/// \brief グラフィックスパイプラインステートの生成
	void CreateGraphicsPipelineState();

private: //メンバ変数
	// カメラ
	Camera* defaultCamera_ = nullptr;

	// DirectXコマンド
	DirectXCommon* dxCommon_ = nullptr;

	// SRVマネージャー
	SrvManager* srvManager_ = nullptr;

	//ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
	//グラフィックスパイプラインステート
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_ = nullptr;

};

