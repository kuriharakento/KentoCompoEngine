#pragma once
#include <string>
#include <wrl.h>
#include "base/GraphicsTypes.h"
#include "d3d12.h"

class Camera;
class DirectXCommon;

class Skybox
{
public:
	~Skybox();
	Skybox();
	void Initialize(DirectXCommon* dxCommon, const std::string& textureFilePath);
	void Update(Camera* camera);
	void Draw();
	void SetTexture(const std::string& textureFilePath)
	{
		CreateModeldata(textureFilePath);
	}

private:
	void CreateModeldata(const std::string& textureFilePath);
	void CreateVertexData();
	void CreateWVPBData();
	void CreateRootSignature();
	void CreatePipelineState();

private:
	DirectXCommon* dxCommon_ = nullptr; // DirectXコマンド
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_; // ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_; // パイプラインステート

	// 頂点データ
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ = {};
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
	VertexData* vertices_ = nullptr;

	// マテリアル
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* material_ = nullptr;

	// テクスチャ
	ModelData modelData_;

	// 座標変換
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_;
	TransformationMatrix* wvpData_ = nullptr;
	Transform transform_ = {
		Vector3(1.0f, 1.0f, 1.0f),
		Vector3(0.0f, 0.0f, 0.0f),
		Vector3(0.0f, 0.0f, 0.0f)
	};
};

