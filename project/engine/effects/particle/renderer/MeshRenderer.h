#pragma once
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

	// IRenderer Interface
	void Initialize(const std::string& texturePath) override;
	void Update(const std::vector<Particle>& particles, CameraManager* camera) override;
	void Draw(DirectXCommon* dxCommon, SrvManager* srvManager) override;
	RendererType GetType() const override { return RendererType::Mesh; }

	void SetTexture(const std::string& texturePath) override;
	void SetGPUMode(bool enable, uint32_t srvIndex, uint32_t count) override;

	// MeshRenderer Specific
	void SetModelPath(const std::string& directory, const std::string& filename);

	//===== プリミティブ設定 =====//

	/**
	 * @brief プリミティブタイプを設定
	 */
	void SetPrimitive(PrimitiveType type);

	/**
	 * @brief プリミティブオプション付きで設定
	 */
	void SetPrimitive(PrimitiveType type, const PrimitiveOptions& options);

	/**
	 * @brief 現在のプリミティブタイプを取得
	 */
	PrimitiveType GetPrimitiveType() const { return primitiveType_; }

	/**
	 * @brief オプションを取得
	 */
	const PrimitiveOptions& GetOptions() const { return options_; }

	/**
	 * @brief スケールを設定
	 */
	void SetScale(float scale) { baseScale_ = scale; }
	float GetScale() const { return baseScale_; }

	/**
	 * @brief ビルボード設定
	 */
	void SetBillboard(bool enable) { useBillboard_ = enable; }
	bool GetBillboard() const { return useBillboard_; }

	void InitializeBuffers(DirectXCommon* dxCommon, SrvManager* srvManager);

private:
	void CreatePrimitiveBuffers(DirectXCommon* dxCommon);
	void RegeneratePrimitive();

	// インスタンシングリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> instanceResource_;
	ParticleGPU* instanceData_ = nullptr;
	uint32_t instanceSrvIndex_ = 0;
	uint32_t instanceCount_ = 0;

	// GPU Mode
	bool isGPUMode_ = false;
	uint32_t gpuSrvIndex_ = 0;
	uint32_t gpuParticleCount_ = 0;

	uint32_t textureIndex_ = 0;

	// マテリアルリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	struct Material* materialData_ = nullptr;

	// プリミティブメッシュデータ
	PrimitiveMesh primitiveMesh_;
	Microsoft::WRL::ComPtr<ID3D12Resource> primitiveVertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> primitiveIndexResource_;
	D3D12_VERTEX_BUFFER_VIEW primitiveVertexView_{};
	D3D12_INDEX_BUFFER_VIEW primitiveIndexView_{};

	// プリミティブ設定
	PrimitiveType primitiveType_ = PrimitiveType::Plane;
	PrimitiveOptions options_{};
	float baseScale_ = 1.0f;
	bool needsRebuild_ = true;
	bool useBillboard_ = false;
};
