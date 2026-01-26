#pragma once
#include <d3d12.h>
#include <string>
#include <wrl.h>
#include <assimp/scene.h>

#include "ModelCommon.h"
#include "base/GraphicsTypes.h"
#include "math/MathUtils.h"

/**
 * @brief スキニングモデルクラス
 * @details ボーンアニメーション付きの3Dモデルを読み込み管理する。
 *          Assimpライブラリを使用してスケルトンとアニメーションデータを解析する。
 */
class SkinnedModel
{
public:
	SkinnedModel() = default;

	/**
	 * @brief コピーコンストラクタ
	 * @param other コピー元のモデル
	 */
	SkinnedModel(const SkinnedModel& other);

	/**
	 * @brief 初期化
	 * @param modelCommon モデル共通部へのポインタ
	 * @param directoryPath モデルファイルのディレクトリパス
	 * @param filename モデルファイル名
	 * @param modelType モデルファイルの拡張子
	 */
	void Initialize(ModelCommon* modelCommon, const std::string& directoryPath,
		const std::string& filename, const std::string& modelType);

	/**
	 * @brief 描画
	 */
	void Draw();

	/**
	 * @brief シャドウマップ用描画
	 */
	void DrawShadow();

	/**
	 * @brief G-Buffer用描画
	 */
	void DrawGBuffer();

public: // アクセッサ
	/**
	 * @brief スキニングモデルデータの取得
	 */
	SkinnedModelData& GetModelData() { return modelData_; }

	/**
	 * @brief スケルトンの取得
	 */
	const Skeleton& GetSkeleton() const { return modelData_.skeleton; }

	/**
	 * @brief アニメーション一覧の取得
	 */
	const std::vector<AnimationClip>& GetAnimations() const { return modelData_.animations; }

	/**
	 * @brief メッシュ数の取得
	 */
	size_t GetMeshCount() const { return meshResources_.size(); }

	/**
	 * @brief マテリアル数の取得
	 */
	size_t GetMaterialCount() const { return modelData_.materials.size(); }

	/**
	 * @brief 頂点総数の取得
	 */
	uint32_t GetTotalVertexCount() const { return totalVertexCount_; }

	/**
	 * @brief スキニング入力バッファの取得（CS用）
	 */
	ID3D12Resource* GetSkinnedVertexInputBuffer() const { return skinnedVertexInputBuffer_.Get(); }

	/**
	 * @brief スキニング出力バッファの取得（CS用）
	 */
	ID3D12Resource* GetSkinnedVertexOutputBuffer() const { return skinnedVertexOutputBuffer_.Get(); }

	/**
	 * @brief 色の取得（最初のマテリアル）
	 * @return 現在の色（RGBA）
	 */
	Vector4 GetColor() const;

	/**
	 * @brief 色の設定（全マテリアルに適用）
	 * @param color 新しい色（RGBA）
	 */
	void SetColor(const Vector4& color);

	/**
	 * @brief ライティングの有効/無効の取得（最初のマテリアル）
	 * @return ライティング有効フラグ
	 */
	bool IsEnableLighting() const;

	/**
	 * @brief ライティングの有効/無効の設定（全マテリアルに適用）
	 * @param enable ライティング有効フラグ
	 */
	void SetEnableLighting(bool enable);

public: // 静的読み込み関数
	/**
	 * @brief スキニングモデルファイルの読み込み
	 * @param directoryPath ファイルのディレクトリパス
	 * @param filename ファイル名
	 * @return スキニングモデルデータ
	 */
	static SkinnedModelData LoadSkinnedModelFile(const std::string& directoryPath, const std::string& filename);

private:
	/**
	 * @brief Assimpからボーン情報を抽出
	 */
	static void ExtractBones(const aiScene* scene, SkinnedModelData& modelData);

	/**
	 * @brief Assimpからアニメーションを抽出
	 */
	static void ExtractAnimations(const aiScene* scene, SkinnedModelData& modelData);

	/**
	 * @brief 頂点ごとのボーンウェイトを抽出
	 */
	static void ExtractBoneWeights(aiMesh* mesh, const Skeleton& skeleton, SkinnedMeshData& meshData);

	/**
	 * @brief ノードの読み取り
	 */
	static Node ReadNode(aiNode* node);

private: // メンバ関数
	/**
	 * @brief メッシュリソースの生成
	 */
	void CreateMeshResources();

	/**
	 * @brief マテリアルリソースの生成
	 */
	void CreateMaterialResources();

	/**
	 * @brief スキニング用バッファの生成
	 */
	void CreateSkinningBuffers();

	/**
	 * @brief 描画設定の初期化
	 */
	void InitializeRenderingSettings();

private:
	/**
	 * @brief メッシュごとのGPUリソース
	 */
	struct MeshResource
	{
		// 頂点バッファビュー
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
		// インデックスバッファリソース
		Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
		// インデックスバッファビュー
		D3D12_INDEX_BUFFER_VIEW indexBufferView{};
		// インデックス数
		uint32_t indexCount = 0;
		// マテリアルインデックス
		uint32_t materialIndex = 0;
		// テクスチャインデックス
		uint32_t textureIndex = 0;
		// 頂点オフセット（全体バッファ内）
		uint32_t vertexOffset = 0;
		// マテリアルバッファ
		Microsoft::WRL::ComPtr<ID3D12Resource> materialBuffer;
		// GPU用マテリアルデータへのポインタ
		Material* gpuMaterial = nullptr;
	};

	// モデル共通部へのポインタ
	ModelCommon* modelCommon_ = nullptr;

	// スキニングモデルデータ
	SkinnedModelData modelData_;

	// メッシュリソース
	std::vector<MeshResource> meshResources_;

	// 全頂点数
	uint32_t totalVertexCount_ = 0;

	// スキニング用入力バッファ（SkinnedVertexData）
	Microsoft::WRL::ComPtr<ID3D12Resource> skinnedVertexInputBuffer_;

	// スキニング用出力バッファ（変形後VertexData）
	Microsoft::WRL::ComPtr<ID3D12Resource> skinnedVertexOutputBuffer_;

	// 現在のリソース状態
	D3D12_RESOURCE_STATES currentResourceState_ = D3D12_RESOURCE_STATE_COMMON;

public:
	/**
	 * @brief リソース状態の取得
	 */
	D3D12_RESOURCE_STATES GetResourceState() const { return currentResourceState_; }

	/**
	 * @brief リソース状態の設定
	 */
	void SetResourceState(D3D12_RESOURCE_STATES state) { currentResourceState_ = state; }
};
