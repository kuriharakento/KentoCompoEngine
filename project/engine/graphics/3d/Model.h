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

/**
 * @brief 3Dモデルクラス
 * @details OBJ/FBXなどの3Dモデルファイルを読み込み、描画するためのクラス。
 *          Assimpライブラリを使用してモデルデータを解析する。
 */
class Model
{
public:
	Model() = default;

	/**
	 * @brief コピーコンストラクタ
	 * @param other コピー元のモデル
	 */
	Model(const Model& other);

	/**
	 * @brief 初期化
	 * @param modelCommon モデル共通部へのポインタ
	 * @param directoryPath モデルファイルのディレクトリパス
	 * @param filename モデルファイル名
	 * @param modelType モデルファイルの拡張子（.obj, .fbxなど）
	 */
	void Initialize(ModelCommon* modelCommon,const std::string& directoryPath, const std::string& filename, const std::string& modelType);

	/**
	 * @brief 描画
	 * @details 頂点バッファとマテリアルを設定して描画コマンドを発行する
	 */
	void Draw();

	/**
	 * @brief .mtlファイルの読み取り
	 * @param directoryPath ファイルのディレクトリパス
	 * @param filename ファイル名
	 * @return マテリアルデータ
	 */
	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

	/**
	 * @brief モデルファイルの読み取り
	 * @param directoryPath ファイルのディレクトリパス
	 * @param filename ファイル名
	 * @return モデルデータ
	 */
	static ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename);

	/**
	 * @brief ノードの読み取り
	 * @param node Assimpのノード
	 * @return ノードデータ
	 */
	static Node ReadNode(aiNode* node);

public: // アクセッサ
	/**
	 * @brief 色の取得
	 * @return 現在の色（RGBA）
	 */
	Vector4 GetColor() const { return materialData_->color; }

	/**
	 * @brief 色の設定
	 * @param color 新しい色（RGBA）
	 */
	void SetColor(const Vector4& color) { materialData_->color = color; }

	/**
	 * @brief ライティングの有効/無効の取得
	 * @return ライティング有効フラグ
	 */
	bool IsEnableLighting() const { return materialData_->enableLighting; }

	/**
	 * @brief ライティングの有効/無効の設定
	 * @param enable ライティング有効フラグ
	 */
	void SetEnableLighting(bool enable) { materialData_->enableLighting = enable; }

	/**
	 * @brief 反射強度の設定
	 * @param shininess 反射強度
	 */
	void SetShininess(float shininess) { materialData_->shininess = shininess; }

	/**
	 * @brief 反射強度の取得
	 * @return 現在の反射強度
	 */
	float GetShininess() const { return materialData_->shininess; }

	/**
	 * @brief モデルデータの取得
	 * @return モデルデータへの参照
	 */
	ModelData& GetModelData() { return modelData_; }

	/**
	 * @brief マテリアルデータの取得
	 * @return マテリアルデータへのポインタ
	 */
	Material* GetMaterialData() { return materialData_; }

	/**
	 * @brief UV移動量の取得
	 * @return UV移動量
	 */
	Vector3 GetUVTranslate() const { return MathUtils::GetTranslateFromMatrix(materialData_->uvTransform); }

	/**
	 * @brief UVスケールの取得
	 * @return UVスケール
	 */
	Vector3 GetUVScale() const { return MathUtils::GetScaleFromMatrix(materialData_->uvTransform); }

	/**
	 * @brief UV回転量の取得
	 * @return UV回転量
	 */
	Vector3 GetUVRotate() const { return MathUtils::GetRotateFromMatrix(materialData_->uvTransform); }

	/**
	 * @brief UV移動量の設定
	 * @param translate 新しいUV移動量
	 */
	void SetUVTranslate(const Vector3& translate) { materialData_->uvTransform = MakeAffineMatrix(GetUVScale(), GetUVRotate(), translate); }

	/**
	 * @brief UVスケールの設定
	 * @param scale 新しいUVスケール
	 */
	void SetUVScale(const Vector3& scale) { materialData_->uvTransform = MakeAffineMatrix(scale, GetUVRotate(), GetUVTranslate()); }

	/**
	 * @brief UV回転量の設定
	 * @param rotate 新しいUV回転量
	 */
	void SetUVRotate(const Vector3& rotate) { materialData_->uvTransform = MakeAffineMatrix(GetUVScale(), rotate, GetUVTranslate()); }

private: // メンバ関数
	/**
	 * @brief 頂点データの生成
	 * @details 頂点バッファを作成してモデルデータをコピーする
	 */
	void CreateVertexData();

	/**
	 * @brief マテリアルデータの生成
	 * @details マテリアルバッファを作成して初期値を設定する
	 */
	void CreateMaterialData();

	/**
	 * @brief 描画設定の初期化
	 * @details 頂点データとマテリアルデータを生成する
	 */
	void InitializeRenderingSettings();

	

private:
	// モデル共通部へのポインタ
	ModelCommon* modelCommon_;

	// モデルデータ
	ModelData modelData_;

	/*-----------------------[ 頂点 ]------------------------*/

	// 頂点バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	// 頂点データへのポインタ
	VertexData* vertexData_ = nullptr;
	// 頂点バッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;

	/*-----------------------[ マテリアル ]------------------------*/

	// マテリアルバッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	// マテリアルデータへのポインタ
	Material* materialData_ = nullptr;

};

