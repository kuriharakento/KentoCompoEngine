#pragma once
/**
 * @file SpriteRenderer.h
 * @brief スプライトパーティクルレンダラー
 * 
 * ビルボードスプライトとしてパーティクルを描画。
 * インスタンシング描画に対応。
 */
#include "IRenderer.h"
#include "effects/particle/Particle.h"
#include "manager/system/SrvManager.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <string>

namespace KCE
{
/**
 * @brief スプライトレンダラー
 */
class SpriteRenderer : public IRenderer
{
public:
	static constexpr uint32_t kMaxParticles = 100000;

	~SpriteRenderer() override;

	/**
	 * @brief 初期化
	 * @param texturePath テクスチャファイルパス
	 */
	void Initialize(const std::string& texturePath) override;

	/**
	 * @brief テクスチャを設定
	 * @param texturePath テクスチャファイルパス
	 */
	void SetTexture(const std::string& texturePath) override;
	void SetEmissiveTexture(const std::string& texturePath) override;
	std::string GetEmissiveTexturePath() const override { return emissiveTexturePath_; }

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
	 * @return Spriteタイプ
	 */
	RendererType GetType() const override { return RendererType::Sprite; }

	/**
	 * @brief ビルボード有効化を設定
	 * @param enabled ビルボード有効化フラグ
	 */
	void SetBillboard(bool enabled) { isBillboard_ = enabled; }

	/**
	 * @brief ビルボード有効化状態を取得
	 * @return ビルボードが有効な場合true
	 */
	bool GetBillboard() const override { return isBillboard_; }

	/** @brief ティントカラーを設定 */
	void SetTintColor(const Vector4& color) override { tintColor_ = color; }
	Vector4 GetTintColor() const override { return tintColor_; }

	/**
	 * @brief GPU描画モードを設定
	 * @param enable GPU描画モード有効化フラグ
	 * @param srvIndex GPUパーティクルバッファのSRVインデックス
	 * @param count GPUパーティクル数
	 */
	void SetGPUMode(bool enable, uint32_t srvIndex, uint32_t count, ID3D12Resource* drawArguments = nullptr) override;

private:
	/**
	 * @brief インスタンスデータを更新
	 * @param particle パーティクルデータ
	 * @param billboardMatrix ビルボード行列
	 * @param camera カメラマネージャー
	 */
	void UpdateInstanceData(const Particle& particle, const Matrix4x4& billboardMatrix, CameraManager* camera);

	/**
	 * @brief バッファを初期化
	 * @param dxCommon DirectXCommonポインタ
	 * @param srvManager SrvManagerポインタ
	 */
	void InitializeBuffers(DirectXCommon* dxCommon, SrvManager* srvManager);

private:
	//===== GPUリソース =====//
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;     ///< 頂点バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_; ///< インスタンシングバッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;   ///< マテリアルバッファリソース
	
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};               ///< 頂点バッファビュー

	ParticleGPU* instancingData_ = nullptr;                     ///< インスタンスデータ（マップ済みポインタ）
	struct Material* materialData_ = nullptr;                   ///< マテリアルデータ（マップ済みポインタ）

	//===== 描画設定 =====//
	uint32_t textureIndex_ = 0;                                 ///< テクスチャのSRVインデックス
	uint32_t emissiveTextureIndex_ = 0;
	std::string emissiveTexturePath_;
	uint32_t instancingSrvIndex_ = SrvManager::kInvalidSrvIndex; ///< インスタンシングバッファのSRVインデックス（未確保=kInvalidSrvIndex）
	uint32_t instanceCount_ = 0;                                ///< 描画するインスタンス数
	bool isBillboard_ = true;                                   ///< ビルボード有効フラグ
	std::string texturePath_;                                   ///< テクスチャファイルパス

public:
	std::string GetTexturePath() const override { return texturePath_; }

	//===== GPUモード設定 =====//
	bool isGPUMode_ = false;                                    ///< GPUシミュレーションモードフラグ
	uint32_t gpuSrvIndex_ = 0;                                  ///< GPUパーティクルバッファのSRVインデックス
	uint32_t gpuParticleCount_ = 0;                             ///< GPUパーティクル数
	ID3D12Resource* gpuDrawArguments_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandSignature> drawCommandSignature_;
	Vector4 tintColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
};
} // namespace KCE
