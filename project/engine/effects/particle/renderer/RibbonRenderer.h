#pragma once
#include "IRenderer.h"
#include "effects/particle/Particle.h"
#include "effects/particle/ParticleTypes.h"
#include "math/Vector2.h"
#include "math/MatrixFunc.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <unordered_map>

/**
 * @brief リボン用頂点データ
 */
struct RibbonVertex
{
	Vector3 position;
	Vector2 texcoord;
	Vector4 color;
};

/**
 * @brief リボンレンダラー
 * 
 * パーティクルの軌跡をリボン状に描画する。
 * 同じRibbonIDを持つパーティクルを連結してトライアングルストリップを生成。
 */
class RibbonRenderer : public IRenderer
{
public:
	static constexpr uint32_t kMaxVertices = 65536;

	~RibbonRenderer() override;

	void Initialize(const std::string& texturePath) override;
	void SetTexture(const std::string& texturePath) override;
	void Update(const std::vector<Particle>& particles, CameraManager* camera) override;
	void Draw(DirectXCommon* dxCommon, SrvManager* srvManager) override;
	RendererType GetType() const override { return RendererType::Ribbon; }

	//===== 設定 =====//

	void SetRibbonWidth(float width) { ribbonWidth_ = width; }
	float GetRibbonWidth() const { return ribbonWidth_; }

	void SetTextureMode(RibbonTextureMode mode) { textureMode_ = mode; }
	RibbonTextureMode GetTextureMode() const { return textureMode_; }

	void SetTileScale(float scale) { tileScale_ = scale; }
	float GetTileScale() const { return tileScale_; }

	void SetMinSegmentLength(float length) { minSegmentLength_ = length; }

	/**
	 * @brief ビルボード設定（リボンでは通常使用しないが、互換性のため提供）
	 */
	void SetBillboard(bool enable) { useBillboard_ = enable; }
	bool GetBillboard() const { return useBillboard_; }

	void InitializeBuffers(DirectXCommon* dxCommon);

private:
	/**
	 * @brief リボンセグメント情報
	 */
	struct RibbonSegment
	{
		Vector3 position;
		float width;
		Vector4 color;
		float age;
	};

	/**
	 * @brief リボンIDごとのセグメントリスト
	 */
	std::unordered_map<uint32_t, std::vector<RibbonSegment>> ribbonSegments_;

	void BuildRibbonMesh(CameraManager* camera);
	void GenerateTriangleStrip(const std::vector<RibbonSegment>& segments, 
	                           const Vector3& cameraPosition,
	                           std::vector<RibbonVertex>& outVertices);

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	RibbonVertex* vertexData_ = nullptr;
	uint32_t vertexCount_ = 0;
	uint32_t textureIndex_ = 0;

	// マテリアルリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	struct Material* materialData_ = nullptr;

	// ビュープロジェクション行列バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> viewProjResource_;
	Matrix4x4* viewProjData_ = nullptr;

	float ribbonWidth_ = 0.5f;
	float tileScale_ = 1.0f;
	float minSegmentLength_ = 0.01f;
	RibbonTextureMode textureMode_ = RibbonTextureMode::Stretch;
	bool useBillboard_ = false;
};
