#pragma once
#include <d3d12.h>
#include <string>
#include <wrl.h>
#include <assimp/scene.h>

#include "ModelCommon.h"
#include "base/GraphicsTypes.h"
#include "math/MathUtils.h"
#include "manager/graphics/SkinnedModelManager.h"

/**
 * @brief スキニングモデルクラス
 * @details ボーンアニメーション付きの3Dモデルのインスタンス。
 *          静的なデータ（頂点・インデックス・ボーン情報）は SkinnedModelManager で共有される。
 *          このクラスは各インスタンス固有のデータ（変形後頂点バッファ、マテリアル）を保持する。
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
	const SkinnedModelData& GetModelData() const { return sharedResource_->modelData; }

	/**
	 * @brief スケルトンの取得
	 */
	const Skeleton& GetSkeleton() const { return sharedResource_->modelData.skeleton; }

	/**
	 * @brief アニメーション一覧の取得
	 */
	const std::vector<AnimationClip>& GetAnimations() const { return sharedResource_->modelData.animations; }

	/**
	 * @brief メッシュ数の取得
	 */
	size_t GetMeshCount() const { return meshResources_.size(); }

	/**
	 * @brief マテリアル数の取得
	 */
	size_t GetMaterialCount() const { return sharedResource_->modelData.materials.size(); }

	/**
	 * @brief 頂点総数の取得
	 */
	uint32_t GetTotalVertexCount() const { return sharedResource_->totalVertexCount; }

	/**
	 * @brief スキニング入力バッファの取得（CS用）
	 */
	ID3D12Resource* GetSkinnedVertexInputBuffer() const { return sharedResource_->inputVertexBuffer.Get(); }

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

	// 共有リソースへのポインタ
	const SkinnedModelSharedResource* sharedResource_ = nullptr;

	// メッシュリソース
	std::vector<MeshResource> meshResources_;

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
