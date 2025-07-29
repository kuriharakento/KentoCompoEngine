#pragma once
#include <d3d12.h>
#include <string>
#include <wrl.h>
#include <assimp/scene.h>

// system
#include "ModelCommon.h"
// math
#include "base/GraphicsTypes.h"
#include "math/MathUtils.h"

class Model
{
public:

	/**
	 * \brief 初期化
	 * \param modelCommon 
	 */
	void Initialize(ModelCommon* modelCommon,const std::string& directoryPath, const std::string& filename);

	/**
	 * \brief 描画
	 */
	void Draw();

	/**
	 * \brief .mtlファイルの読み取り
	 * \param directoryPath
	 * \param filename
	 * \return
	 */
	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

	/**
	 * \brief .objファイルの読み取り
	 * \param
	 * \param
	 * \return
	 */
	static ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename);

	/**
	 * \brief ノードの読み取り
	 * \param node
	 * \return
	 */
	static Node ReadNode(aiNode* node);

public: //アクセッサ
	Vector4 GetColor() const { return materialData_->color; }
	void SetColor(const Vector4& color) { materialData_->color = color; }

	//ライティングの有効無効
	bool IsEnableLighting() const { return materialData_->enableLighting; }
	void SetEnableLighting(bool enable) { materialData_->enableLighting = enable; }

	//反射強度
	void SetShininess(float shininess) { materialData_->shininess = shininess; }
	float GetShininess() const { return materialData_->shininess; }

	//モデルデータ
	ModelData& GetModelData() { return modelData_; }

	//マテリアルデータ
	Material* GetMaterialData() { return materialData_; }
	Vector3 GetUVTranslate() const { return MathUtils::GetMatrixTranslate(materialData_->uvTransform); }
	Vector3 GetUVScale() const { return MathUtils::GetMatrixScale(materialData_->uvTransform); }
	Vector3 GetUVRotate() const { return MathUtils::GetMatrixRotate(materialData_->uvTransform); }
	void SetUVTranslate(const Vector3& translate) { materialData_->uvTransform = MakeAffineMatrix(GetUVScale(), GetUVRotate(), translate); }
	void SetUVScale(const Vector3& scale) { materialData_->uvTransform = MakeAffineMatrix(scale, GetUVRotate(), GetUVTranslate()); }
	void SetUVRotate(const Vector3& rotate) { materialData_->uvTransform = MakeAffineMatrix(GetUVScale(), rotate, GetUVTranslate()); }

private: //メンバ関数
	/**
	 * \brief 頂点データの生成
	 */
	void CreateVertexData();

	/**
	 * \brief マテリアルデータの生成
	 */
	void CreateMaterialData();

	/**
	 * \brief 描画設定の初期化
	 */
	void InitializeRenderingSettings();

	

private:
	//モデルコマンド
	ModelCommon* modelCommon_;

	//Objファイルのデータ
	ModelData modelData_;

	/*-----------------------[ 頂点 ]------------------------*/

	//バッファのリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	//データ
	VertexData* vertexData_ = nullptr;
	//頂点バッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;

	/*-----------------------[ マテリアル ]------------------------*/

	//バッファのリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	//データ
	Material* materialData_ = nullptr;

};

