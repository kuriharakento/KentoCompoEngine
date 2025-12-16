#pragma once
/**
 * @file MeshRenderer.h
 * @brief メッシュパーティクルレンダラー
 * 
 * プリミティブ形状（Plane, Sphere, Cube, Cone等）を
 * パーティクルとしてインスタンシング描画。
 */
#include "IRenderer.h"
#include "effects/particle/Particle.h"
#include "effects/particle/ParticleTypes.h"
#include "effects/particle/primitive/PrimitiveGenerator.h"
#include <d3d12.h>
#include <wrl/client.h>

/**
 * @brief メッシュレンダラー
 * 
 * プリミティブ形状をパーティクルとして描画する。
 * インスタンシング描画で大量のメッシュパーティクルを効率的に描画。
 */
class MeshRenderer : public IRenderer
{
public:
	static constexpr uint32_t kMaxInstances = 10000;

	~MeshRenderer() override;

	/**
	 * @brief 初期化
	 * @param texturePath テクスチャファイルパス
	 */
	void Initialize(const std::string& texturePath) override;

	/**
	 * @brief パーティクルデータを更新
	 * @param particles パーティクルリスト
	 * @param camera カメラマネージャー
	 */
	void Update(const std::vector<Particle>& particles, CameraManager* camera) override;

	/**
	 * @brief 描画
	 * @param dxCommon DirectXCommonポインタ
	 * @param srvManager SrvManagerポインタ
	 */
	void Draw(DirectXCommon* dxCommon, SrvManager* srvManager) override;

	/**
	 * @brief レンダラータイプを取得
	 * @return Meshタイプ
	 */
	RendererType GetType() const override { return RendererType::Mesh; }

	/**
	 * @brief テクスチャを設定
	 * @param texturePath テクスチャファイルパス
	 */
	void SetTexture(const std::string& texturePath) override;

	/**
	 * @brief GPU描画モードを設定
	 * @param enable GPU描画モード有効化フラグ
	 * @param srvIndex GPUパーティクルバッファのSRVインデックス
	 * @param count GPUパーティクル数
	 */
	void SetGPUMode(bool enable, uint32_t srvIndex, uint32_t count) override;

	/**
	 * @brief モデルパスを設定（外部メッシュファイル用）
	 * @param directory ディレクトリパス
	 * @param filename ファイル名
	 */
	void SetModelPath(const std::string& directory, const std::string& filename);

	//===== プリミティブ設定 =====//

	/**
	 * @brief プリミティブタイプを設定
	 * @param type プリミティブタイプ
	 */
	void SetPrimitive(PrimitiveType type);

	/**
	 * @brief プリミティブをオプション付きで設定
	 * @param type プリミティブタイプ
	 * @param options プリミティブ生成オプション
	 */
	void SetPrimitive(PrimitiveType type, const PrimitiveOptions& options);

	/**
	 * @brief 現在のプリミティブタイプを取得
	 * @return プリミティブタイプ
	 */
	PrimitiveType GetPrimitiveType() const { return primitiveType_; }

	/**
	 * @brief プリミティブオプションを取得
	 * @return プリミティブオプション
	 */
	const PrimitiveOptions& GetOptions() const { return options_; }

	/**
	 * @brief 基本スケールを設定
	 * @param scale スケール値
	 */
	void SetScale(float scale) { baseScale_ = scale; }

	/**
	 * @brief 基本スケールを取得
	 * @return スケール値
	 */
	float GetScale() const { return baseScale_; }

	/**
	 * @brief ビルボード設定
	 * @param enable ビルボード有効化フラグ
	 */
	void SetBillboard(bool enable) { useBillboard_ = enable; }

	/**
	 * @brief ビルボード設定を取得
	 * @return ビルボード有効フラグ
	 */
	bool GetBillboard() const { return useBillboard_; }

	/**
	 * @brief ティントカラーを設定
	 * @param color ティントカラー（RGBA）
	 */
	void SetTintColor(const Vector4& color) { tintColor_ = color; }

	/**
	 * @brief ティントカラーを取得
	 * @return ティントカラー（RGBA）
	 */
	Vector4 GetTintColor() const { return tintColor_; }

	/**
	 * @brief バッファを初期化
	 * @param dxCommon DirectXCommonポインタ
	 * @param srvManager SrvManagerポインタ
	 */
	void InitializeBuffers(DirectXCommon* dxCommon, SrvManager* srvManager);

private:
	/**
	 * @brief プリミティブ用のバッファを作成
	 * @param dxCommon DirectXCommonポインタ
	 */
	void CreatePrimitiveBuffers(DirectXCommon* dxCommon);

	/**
	 * @brief プリミティブメッシュを再生成
	 */
	void RegeneratePrimitive();

	//===== インスタンシングリソース =====//
	Microsoft::WRL::ComPtr<ID3D12Resource> instanceResource_;   ///< インスタンシングバッファリソース
	ParticleGPU* instanceData_ = nullptr;                       ///< インスタンスデータ（マップ済みポインタ）
	uint32_t instanceSrvIndex_ = 0;                             ///< インスタンスバッファのSRVインデックス
	uint32_t instanceCount_ = 0;                                ///< 描画するインスタンス数

	//===== GPUモード設定 =====//
	bool isGPUMode_ = false;                                    ///< GPUシミュレーションモードフラグ
	uint32_t gpuSrvIndex_ = 0;                                  ///< GPUパーティクルバッファのSRVインデックス
	uint32_t gpuParticleCount_ = 0;                             ///< GPUパーティクル数

	uint32_t textureIndex_ = 0;                                 ///< テクスチャのSRVインデックス
	std::string texturePath_;                                   ///< テクスチャファイルパス

public:
	/**
	 * @brief テクスチャパスを取得
	 * @return テクスチャファイルパス
	 */
	std::string GetTexturePath() const override { return texturePath_; }

private:

	//===== マテリアルリソース =====//
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;   ///< マテリアルバッファリソース
	struct Material* materialData_ = nullptr;                   ///< マテリアルデータ（マップ済みポインタ）

	//===== プリミティブメッシュデータ =====//
	PrimitiveMesh primitiveMesh_;                               ///< 生成されたプリミティブメッシュ
	Microsoft::WRL::ComPtr<ID3D12Resource> primitiveVertexResource_; ///< プリミティブ頂点バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> primitiveIndexResource_;  ///< プリミティブインデックスバッファ
	D3D12_VERTEX_BUFFER_VIEW primitiveVertexView_{};            ///< プリミティブ頂点バッファビュー
	D3D12_INDEX_BUFFER_VIEW primitiveIndexView_{};              ///< プリミティブインデックスバッファビュー

	//===== プリミティブ設定 =====//
	PrimitiveType primitiveType_ = PrimitiveType::Plane;        ///< プリミティブ形状タイプ
	PrimitiveOptions options_{};                                ///< プリミティブ生成オプション
	float baseScale_ = 1.0f;                                    ///< 基本スケール
	bool needsRebuild_ = true;                                  ///< プリミティブ再生成フラグ
	bool useBillboard_ = false;                                 ///< ビルボード有効フラグ
	Vector4 tintColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };           ///< ティントカラー（RGBA）
};
