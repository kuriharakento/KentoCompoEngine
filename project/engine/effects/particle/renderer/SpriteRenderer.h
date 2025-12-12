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
#include <d3d12.h>
#include <wrl/client.h>
#include <string>

/**
 * @brief スプライトレンダラー
 */
class SpriteRenderer : public IRenderer
{
public:
	static constexpr uint32_t kMaxParticles = 10000;

	~SpriteRenderer() override;
	void Initialize(const std::string& texturePath) override;
	void SetTexture(const std::string& texturePath) override;
	void Update(const std::vector<Particle>& particles, CameraManager* camera) override;
	void Draw(DirectXCommon* dxCommon, SrvManager* srvManager) override;
	RendererType GetType() const override { return RendererType::Sprite; } // 22

	void SetBillboard(bool enabled) { isBillboard_ = enabled; }
	void SetGPUMode(bool enable, uint32_t srvIndex, uint32_t count) override; 

private:
	void UpdateInstanceData(const Particle& particle, const Matrix4x4& billboardMatrix, CameraManager* camera);
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
	uint32_t instancingSrvIndex_ = 0;                           ///< インスタンシングバッファのSRVインデックス
	uint32_t instanceCount_ = 0;                                ///< 描画するインスタンス数
	bool isBillboard_ = true;                                   ///< ビルボード有効フラグ
	std::string texturePath_;                                   ///< テクスチャファイルパス

public:
	std::string GetTexturePath() const override { return texturePath_; }

	//===== GPUモード設定 =====//
	bool isGPUMode_ = false;                                    ///< GPUシミュレーションモードフラグ
	uint32_t gpuSrvIndex_ = 0;                                  ///< GPUパーティクルバッファのSRVインデックス
	uint32_t gpuParticleCount_ = 0;                             ///< GPUパーティクル数
};
