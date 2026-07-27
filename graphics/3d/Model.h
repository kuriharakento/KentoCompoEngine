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

namespace KCE
{
/**
 * @brief 3Dモデルクラス
 * @details OBJ/FBXなどの3Dモデルファイルを読み込み、描画するためのクラス。
 *          Assimpライブラリを使用してモデルデータを解析する。
 *          マルチメッシュ、マルチマテリアル、インデックスバッファに対応。
 */
class Model
{
public:
	Model() = default;

	/**
	 * @brief メッシュごとのGPUリソース
	 */
	struct MeshResource
	{
		// 頂点バッファリソース
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
		// インデックスバッファリソース
		Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
		// マテリアルバッファリソース
		Microsoft::WRL::ComPtr<ID3D12Resource> materialBuffer;
		// 頂点バッファビュー
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
		// インデックスバッファビュー
		D3D12_INDEX_BUFFER_VIEW indexBufferView{};
		// インデックス数
		uint32_t indexCount = 0;
		// 使用するマテリアルのインデックス
		uint32_t materialIndex = 0;
		// テクスチャインデックス
		uint32_t textureIndex = 0;
		// GPU用マテリアルデータへのポインタ
		Material* gpuMaterial = nullptr;
	};

	/**
	 * @brief 全メッシュリソースの取得
	 */
	const std::vector<MeshResource>& GetMeshResources() const { return meshResources_; }

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
	 * @brief シャドウマップ用描画
	 * @details 頂点バッファのみを設定して描画コマンドを発行する（マテリアル設定なし）
	 */
	void DrawShadow();

	/**
	 * @brief G-Buffer用描画（ディファードレンダリング）
	 * @details GBufferPipelineのルートシグネチャに合わせてバインディングを行う
	 */
	void DrawGBuffer();

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
	 * @brief 色の取得（最初のマテリアル）
	 * @return 現在の色（RGBA）
	 */
	Vector4 GetColor() const { return meshResources_.empty() ? Vector4(1,1,1,1) : meshResources_[0].gpuMaterial->color; }

	/**
	 * @brief 色の設定（全マテリアルに適用）
	 * @param color 新しい色（RGBA）
	 */
	void SetColor(const Vector4& color);

	/**
	 * @brief ライティングの有効/無効の取得（最初のマテリアル）
	 * @return ライティング有効フラグ
	 */
	bool IsEnableLighting() const { return meshResources_.empty() ? true : meshResources_[0].gpuMaterial->enableLighting; }

	/**
	 * @brief ライティングの有効/無効の設定（全マテリアルに適用）
	 * @param enable ライティング有効フラグ
	 */
	void SetEnableLighting(bool enable);

	/**
	 * @brief 反射強度の設定（全マテリアルに適用）
	 * @param shininess 反射強度
	 */
	void SetShininess(float shininess);

	/**
	 * @brief 反射強度の取得（最初のマテリアル）
	 * @return 現在の反射強度
	 */
	float GetShininess() const { return meshResources_.empty() ? 30.0f : meshResources_[0].gpuMaterial->shininess; }

	/**
	 * @brief モデルデータの取得
	 * @return モデルデータへの参照
	 */
	ModelData& GetModelData() { return modelData_; }

	/**
	 * @brief マテリアルデータの取得（最初のメッシュのマテリアル）
	 * @return マテリアルデータへのポインタ
	 */
	Material* GetMaterialData() { return meshResources_.empty() ? nullptr : meshResources_[0].gpuMaterial; }

	/**
	 * @brief UV移動量の取得
	 * @return UV移動量
	 */
	Vector3 GetUVTranslate() const;

	/**
	 * @brief UVスケールの取得
	 * @return UVスケール
	 */
	Vector3 GetUVScale() const;

	/**
	 * @brief UV回転量の取得
	 * @return UV回転量
	 */
	Vector3 GetUVRotate() const;

	/**
	 * @brief UV移動量の設定（全マテリアルに適用）
	 * @param translate 新しいUV移動量
	 */
	void SetUVTranslate(const Vector3& translate);

	/**
	 * @brief UVスケールの設定（全マテリアルに適用）
	 * @param scale 新しいUVスケール
	 */
	void SetUVScale(const Vector3& scale);

	/**
	 * @brief UV回転量の設定（全マテリアルに適用）
	 * @param rotate 新しいUV回転量
	 */
	void SetUVRotate(const Vector3& rotate);

	/**
	 * @brief メッシュ数の取得
	 * @return メッシュ数
	 */
	size_t GetMeshCount() const { return meshResources_.size(); }

	/**
	 * @brief マテリアル数の取得
	 * @return マテリアル数
	 */
	size_t GetMaterialCount() const { return modelData_.materials.size(); }

private: // メンバ関数
	/**
	 * @brief メッシュリソースの生成
	 * @details 全メッシュの頂点・インデックスバッファを作成
	 */
	void CreateMeshResources();

	/**
	 * @brief マテリアルリソースの生成
	 * @details 全マテリアルのGPUバッファを作成
	 */
	void CreateMaterialResources();

	/**
	 * @brief 描画設定の初期化
	 * @details メッシュリソースとマテリアルリソースを生成する
	 */
	void InitializeRenderingSettings();

private:

	// モデル共通部へのポインタ
	ModelCommon* modelCommon_ = nullptr;

	// モデルデータ
	ModelData modelData_;

	// メッシュリソース（マルチメッシュ対応）
	std::vector<MeshResource> meshResources_;
};
} // namespace KCE
