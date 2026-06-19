#pragma once
/**
 * @file TrailRenderer.h
 * @brief トレイル（軌跡）パーティクルレンダラー
 *
 * パーティクルの移動軌跡をリボン状に描画する。
 * Niagaraライクなトレイルエフェクトを実現。
 */
#include "IRenderer.h"
#include "effects/particle/Particle.h"
#include "effects/particle/ParticleTypes.h"
#include "math/Vector2.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/MatrixFunc.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <deque>

class CameraManager;
class DirectXCommon;
class SrvManager;

/**
 * @brief トレイル用頂点データ
 */
struct TrailVertex
{
	Vector3 position;
	Vector2 texcoord;
	Vector4 color;
};

/**
 * @brief トレイルセグメント情報
 */
struct TrailSegment
{
	Vector3 position;         // 位置
	Vector3 direction;        // 方向（次のセグメントへのベクトル）
	float width;              // 幅
	Vector4 color;            // 色
	float age;                // 生成からの経過時間
	float lifetime;           // このセグメントの寿命
};

/**
 * @brief トレイルデータ（パーティクルごと）
 */
struct TrailData
{
	std::deque<TrailSegment> segments;  // セグメントリスト（新しい順）
	float lastRecordTime = 0.0f;        // 最後に記録した時刻
	bool isActive = true;               // アクティブかどうか
};

/**
 * @brief トレイルマテリアル
 */
struct TrailMaterial
{
	Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
	int32_t enableLighting = 0;
	int32_t useTextureColor = 1;
	float padding[2] = {};
	Matrix4x4 uvTransform = MakeIdentity4x4();
};

/**
 * @brief トレイルレンダラー
 *
 * パーティクルの軌跡をリボン状に描画する。
 * 各パーティクルの位置履歴を保存し、スムーズなトレイルメッシュを生成。
 */
class TrailRenderer : public IRenderer
{
public:
	static constexpr uint32_t kMaxVertices = 65536;
	static constexpr uint32_t kMaxTrails = 1000;

	~TrailRenderer() override;

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
	 * @return Ribbonタイプ（互換性のため）
	 */
	RendererType GetType() const override { return RendererType::Ribbon; }

	/**
	 * @brief テクスチャパスを取得
	 */
	std::string GetTexturePath() const override { return texturePath_; }

	//===== 設定 =====//

	/** @brief トレイル幅を設定 */
	void SetTrailWidth(float width) { trailWidth_ = width; }
	float GetTrailWidth() const { return trailWidth_; }

	/** @brief トレイル寿命を設定 */
	void SetTrailLifetime(float lifetime) { trailLifetime_ = lifetime; }
	float GetTrailLifetime() const { return trailLifetime_; }

	/** @brief 記録間隔を設定（秒） */
	void SetRecordInterval(float interval) { recordInterval_ = interval; }
	float GetRecordInterval() const { return recordInterval_; }

	/** @brief 最小セグメント距離を設定 */
	void SetMinSegmentDistance(float distance) { minSegmentDistance_ = distance; }
	float GetMinSegmentDistance() const { return minSegmentDistance_; }

	/** @brief テクスチャモードを設定 */
	void SetTextureMode(RibbonTextureMode mode) { textureMode_ = mode; }
	RibbonTextureMode GetTextureMode() const { return textureMode_; }

	/** @brief タイルスケールを設定 */
	void SetTileScale(float scale) { tileScale_ = scale; }
	float GetTileScale() const { return tileScale_; }

	/** @brief ビルボード設定 */
	void SetBillboard(bool enable) { useBillboard_ = enable; }
	bool GetBillboard() const { return useBillboard_; }

	/** @brief 幅フェード（先端に向かって細くなる）を設定 */
	void SetWidthFade(bool enable) { widthFade_ = enable; }
	bool GetWidthFade() const { return widthFade_; }

	/** @brief アルファフェードを設定 */
	void SetAlphaFade(bool enable) { alphaFade_ = enable; }
	bool GetAlphaFade() const { return alphaFade_; }

	/** @brief トレイルをクリア */
	void ClearTrails() { trails_.clear(); }

private:
	/**
	 * @brief バッファを初期化
	 */
	void InitializeBuffers(DirectXCommon* dxCommon, SrvManager* srvManager);

	/**
	 * @brief トレイルデータを更新
	 */
	void UpdateTrails(const std::vector<Particle>& particles, float deltaTime);

	/**
	 * @brief トレイルメッシュを構築（旧方式、互換性用）
	 */
	void BuildTrailMesh(CameraManager* camera);

	/**
	 * @brief パーティクルを接続してリボンメッシュを構築（新方式・RibbonIdグループ対応）
	 */
	void BuildRibbonFromParticles(const std::vector<Particle>& particles, CameraManager* camera);

	/**
	 * @brief 単一リボングループから頂点を生成
	 */
	void GenerateRibbonVertices(
		const std::vector<const Particle*>& group,
		const Vector3& cameraPosition,
		std::vector<TrailVertex>& outVertices
	);

	/**
	 * @brief セグメントからトライアングルストリップを生成
	 */
	void GenerateTriangleStrip(
		const std::deque<TrailSegment>& segments,
		const Vector3& cameraPosition,
		std::vector<TrailVertex>& outVertices
	);

	/**
	 * @brief 2点間を補間してセグメントを追加
	 */
	void InterpolateSegments(
		const TrailSegment& from,
		const TrailSegment& to,
		std::vector<TrailSegment>& outSegments
	);

private:
	//===== GPUリソース =====//
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	TrailVertex* vertexData_ = nullptr;
	uint32_t vertexCount_ = 0;

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	TrailMaterial* materialData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> viewProjResource_;
	Matrix4x4* viewProjData_ = nullptr;

	//===== 描画設定 =====//
	uint32_t textureIndex_ = 0;
	std::string texturePath_;

	//===== トレイル設定 =====//
	float trailWidth_ = 0.5f;
	float trailLifetime_ = 1.0f;
	float recordInterval_ = 0.016f;  // 約60fps
	float minSegmentDistance_ = 0.05f;
	float tileScale_ = 1.0f;
	RibbonTextureMode textureMode_ = RibbonTextureMode::Stretch;
	bool useBillboard_ = true;
	bool widthFade_ = true;
	bool alphaFade_ = true;

	//===== トレイルデータ =====//
	std::unordered_map<uint32_t, TrailData> trails_;
	float currentTime_ = 0.0f;
	float lastDeltaTime_ = 0.016f;
};
