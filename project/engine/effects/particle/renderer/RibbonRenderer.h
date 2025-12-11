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
 * @brief リボン用マテリアル（シェーダーと一致する構造）
 */
struct RibbonMaterial
{
	Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
	int32_t enableLighting = 0;
	int32_t useTextureColor = 0;  // 0: 頂点カラーのみ, 1: テクスチャカラーも使用
	float padding[2] = {};
	Matrix4x4 uvTransform = MakeIdentity4x4();
};

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

	void SetAlphaThreshold(float threshold) { alphaThreshold_ = threshold; }
	float GetAlphaThreshold() const { return alphaThreshold_; }

	void SetUseTextureColor(bool use) { useTextureColor_ = use; }
	bool GetUseTextureColor() const { return useTextureColor_; }

	void SetMinSegmentLength(float length) { minSegmentLength_ = length; }
	
	void SetMaxSegmentDistance(float distance) { maxSegmentDistance_ = distance; }
	float GetMaxSegmentDistance() const { return maxSegmentDistance_; }
	
	void SetEnableInterpolation(bool enable) { enableInterpolation_ = enable; }
	bool GetEnableInterpolation() const { return enableInterpolation_; }
	
	// トレイル設定
	void SetPointsPerSecond(float pps) { pointsPerSecond_ = pps; }
	float GetPointsPerSecond() const { return pointsPerSecond_; }
	
	void SetTrailLifetime(float lifetime) { trailLifetime_ = lifetime; }
	float GetTrailLifetime() const { return trailLifetime_; }

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
		Vector3 tangent;          // 接線方向（生成時に固定）
		float width;
		Vector4 color;
		float normalizedAge;      // 0.0(新しい) ～ 1.0(古い)
		float timestamp;          // 記録された時刻
		bool isInterpolated = false;  // 補間で生成されたセグメントか
	};

	/**
	 * @brief リボントレイル（位置履歴）
	 */
	struct RibbonTrail
	{
		std::vector<RibbonSegment> segments;
		float lastAddTime = 0.0f;
		bool isActive = true;
	};

	/**
	 * @brief リボンIDごとのトレイル履歴
	 */
	std::unordered_map<uint32_t, RibbonTrail> ribbonTrails_;
	
	// 一時的なセグメントリスト（描画用）
	std::unordered_map<uint32_t, std::vector<RibbonSegment>> ribbonSegments_;

	/**
	 * @brief パーティクルごとの位置履歴（トレイル描画用）
	 */
	struct ParticleTrailHistory {
		std::vector<Vector3> positions;      // 位置履歴
		std::vector<Vector4> colors;         // 色履歴
		float lastRecordTime = 0.0f;         // 最後に記録した時刻（particle.age）
	};
	std::unordered_map<uint32_t, ParticleTrailHistory> particleHistories_;

	void BuildRibbonMesh(CameraManager* camera);
	void BuildRibbonMeshFromTrails(CameraManager* camera);
	void UpdateTrails(const std::vector<Particle>& particles, float deltaTime);
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
	RibbonMaterial* materialData_ = nullptr;

	// ビュープロジェクション行列バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> viewProjResource_;
	Matrix4x4* viewProjData_ = nullptr;

	float ribbonWidth_ = 0.5f;
	float tileScale_ = 1.0f;
	float minSegmentLength_ = 0.01f;
	float maxSegmentDistance_ = 0.02f; // この距離を超えたら補間点を作成（小さいほど滑らか）
	float alphaThreshold_ = 0.05f; // この値以下のアルファを持つセグメントは描画しない
	bool useTextureColor_ = false; // true: テクスチャカラーも使用, false: 頂点カラーのみ
	bool enableInterpolation_ = false; // セグメント補間を無効（スポーン時補間を使用）
	RibbonTextureMode textureMode_ = RibbonTextureMode::Stretch;
	bool useBillboard_ = true; // デフォルトでビルボードON
	
	// トレイル設定
	float pointsPerSecond_ = 120.0f; // 1秒あたりに記録する点の数（高いほど滑らか）
	float trailLifetime_ = 2.0f;     // トレイルの寿命（秒）
	float currentTime_ = 0.0f;       // 現在時刻
	
	// セグメント補間
	void InterpolateSegments(std::vector<RibbonSegment>& segments);
	
	// 先頭セグメントとの補間（新規追加時のみ）
	std::vector<RibbonSegment> InterpolateWithHead(const RibbonSegment& head, const RibbonSegment& newSegment);
};
