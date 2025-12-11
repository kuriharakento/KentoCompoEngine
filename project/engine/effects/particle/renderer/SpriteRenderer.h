#pragma once
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
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	ParticleGPU* instancingData_ = nullptr;
	struct Material* materialData_ = nullptr;

	uint32_t textureIndex_ = 0;
	uint32_t instancingSrvIndex_ = 0;
	uint32_t instanceCount_ = 0;
	bool isBillboard_ = true;
	std::string texturePath_;

public:
	std::string GetTexturePath() const override { return texturePath_; }

	// GPU Mode
	bool isGPUMode_ = false;
	uint32_t gpuSrvIndex_ = 0;
	uint32_t gpuParticleCount_ = 0;
};
