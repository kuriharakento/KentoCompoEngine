#pragma once
#include <string>
#include <wrl.h>
#include "base/GraphicsTypes.h"
#include "d3d12.h"

class Camera;
class DirectXCommon;

/**
 * @brief スカイボックスクラス
 * @details 3D空間の背景として描画されるキューブ型のスカイボックスを管理する
 */
class Skybox
{
public:
	/**
	 * @brief デストラクタ
	 * @details リソースの解放を行う
	 */
	~Skybox();

	/**
	 * @brief コンストラクタ
	 */
	Skybox();

	/**
	 * @brief 初期化
	 * @param dxCommon DirectXCommonへのポインタ
	 * @param textureFilePath スカイボックステクスチャのファイルパス
	 */
	void Initialize(DirectXCommon* dxCommon, const std::string& textureFilePath);

	/**
	 * @brief 更新
	 * @param camera カメラへのポインタ
	 */
	void Update(Camera* camera);

	/**
	 * @brief 描画
	 * @details スカイボックスをコマンドリストに描画する
	 */
	void Draw();

	/**
	 * @brief テクスチャの設定
	 * @param textureFilePath 新しいテクスチャのファイルパス
	 */
	void SetTexture(const std::string& textureFilePath)
	{
		CreateModeldata(textureFilePath);
	}

private:
	/**
	 * @brief モデルデータの作成
	 * @param textureFilePath テクスチャのファイルパス
	 */
	void CreateModeldata(const std::string& textureFilePath);

	/**
	 * @brief 頂点データの作成
	 * @details キューブ形状の頂点データを生成する
	 */
	void CreateVertexData();

	/**
	 * @brief WVP行列データの作成
	 * @details 座標変換用のバッファを作成する
	 */
	void CreateWVPBData();

	/**
	 * @brief ルートシグネチャの作成
	 * @details スカイボックス用のルートシグネチャを構築する
	 */
	void CreateRootSignature();

	/**
	 * @brief パイプラインステートの作成
	 * @details スカイボックス用のパイプラインステートを構築する
	 */
	void CreatePipelineState();

private:
	// DirectXCommonへのポインタ
	DirectXCommon* dxCommon_ = nullptr;
	// ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	// パイプラインステート
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

	// 頂点バッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ = {};
	// 頂点バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
	// 頂点データへのポインタ
	VertexData* vertices_ = nullptr;

	// マテリアルリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	// マテリアルデータへのポインタ
	Material* material_ = nullptr;

	// テクスチャインデックス
	uint32_t textureIndex_ = 0;
	// 頂点数
	size_t vertexCount_ = 0;

	// 座標変換行列リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_;
	// 座標変換行列データへのポインタ
	TransformationMatrix* wvpData_ = nullptr;
	// Transform情報
	Transform transform_ = {
		Vector3(1.0f, 1.0f, 1.0f),
		Vector3(0.0f, 0.0f, 0.0f),
		Vector3(0.0f, 0.0f, 0.0f)
	};
};


