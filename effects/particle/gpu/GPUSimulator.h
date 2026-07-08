#pragma once
/**
 * @file GPUSimulator.h
 * @brief GPUパーティクルシミュレーター
 * 
 * コンピュートシェーダーによるパーティクルシミュレーション。
 * エミッターごとのバッファを管理し、GPUで並列計算。
 */
#include "effects/particle/Particle.h"
#include "manager/system/SrvManager.h"
#include "math/Vector3.h"
#include "math/MatrixFunc.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <vector>

class DirectXCommon;
class SrvManager;
class CameraManager;
enum class RendererType;


/**
 * @brief GPUパーティクル定数バッファデータ構造体
 * 
 * GPUシミュレーション用の定数パラメータを格納。
 * シェーダーに渡される。
 */
struct alignas(16) GPUParticleConstants
{
	// Block 0 (16 bytes)
	float deltaTime;           ///< 経過時間（秒）
	float totalTime;           ///< 総経過時間（秒）
	uint32_t particleCount;    ///< 現在のアクティブなパーティクル数
	uint32_t maxParticles;     ///< バッファの最大パーティクル数

	// Block 1 (16 bytes)
	Vector3 emitterPosition;   ///< エミッター位置
	uint32_t isBillboard;      ///< ビルボード有効フラグ（0=無効, 1=有効）

	// Block 2 (16 bytes)
	Vector3 gravity;           ///< 重力加速度
	uint32_t simulationSpace;  ///< シミュレーション空間（0=World, 1=Local）

	// Block 3-6 (64 bytes)
	Matrix4x4 emitterWorld;    ///< エミッターのワールド行列

	// Block 7 (16 bytes)
	uint32_t spawnCount;       ///< 新規発生パーティクル数
	float paddingSpawn[3];

	// Block 8 (16 bytes)
	uint32_t hasDrag;          ///< Drag有効フラグ
	float dragMin;             ///< 最小Drag
	float dragMax;             ///< 最大Drag
	float paddingDrag;
	
	// Block 9 (16 bytes)
	uint32_t hasColorFade;     ///< ColorFade有効フラグ
	uint32_t colorFadeUseInitial; ///< 初期カラー使用フラグ
	uint32_t colorFadeEasing;  ///< イージングタイプ (EasingType)
	float paddingCF;

	// Block 10 (16 bytes)
	Vector4 colorFadeStart;    ///< 開始カラー

	// Block 11 (16 bytes)
	Vector4 colorFadeEnd;      ///< 終了カラー
	
	// Block 12 (16 bytes)
	uint32_t hasScaleOL;       ///< ScaleOverLifetime有効フラグ
	uint32_t scaleOLEasing;    ///< イージングタイプ (EasingType)
	float paddingScaleOL[2];

	// Block 13 (16 bytes)
	Vector3 scaleOLStart;      ///< 開始スケール
	float paddingS1;

	// Block 14 (16 bytes)
	Vector3 scaleOLEnd;        ///< 終了スケール
	float paddingS2;

	// Block 15 (16 bytes)
	uint32_t hasNoise;         ///< Noise有効フラグ
	float noiseStrength;       ///< Noise強度
	float noiseFrequency;      ///< Noise周波数
	float paddingNoise;

	// Block 16 (16 bytes)
	uint32_t hasRotationOL;    ///< RotationOverLifetime有効フラグ
	float rotOLStartSpeed;     ///< 開始回転速度
	float rotOLEndSpeed;       ///< 終了回転速度
	uint32_t rotOLEasing;      ///< イージングタイプ (EasingType)

	// Block 17 (16 bytes)
	uint32_t hasAlphaFade;     ///< AlphaFade有効フラグ
	float alphaFadeStart;      ///< 開始アルファ
	float alphaFadeEnd;        ///< 終了アルファ
	uint32_t alphaFadeEaseIn;  ///< EaseInフラグ

	// Block 18 (16 bytes)
	uint32_t alphaFadeEaseOut; ///< EaseOutフラグ
	float paddingAlpha[3];

	// Block 19 (16 bytes)
	uint32_t hasVelocityOL;    ///< VelocityOverLifetime有効フラグ
	float velocityOLStart;     ///< 開始乗数
	float velocityOLEnd;       ///< 終了乗数
	float paddingVelocityOL;

	// Block 20 (16 bytes)
	uint32_t hasStretchByVelocity; ///< StretchByVelocity有効フラグ
	float stretchFactor;       ///< ストレッチ係数
	float minStretch;          ///< 最小ストレッチ
	float maxStretch;          ///< 最大ストレッチ

	// Block 21 (16 bytes)
	uint32_t stretchPreserveVolume; ///< 体積維持フラグ (0=無効, 1=有効)
	float paddingStretch[3];

	// Block 22 (16 bytes)
	uint32_t hasFlicker;       ///< Flicker有効フラグ
	float flickerFrequency;    ///< 周波数
	float flickerMinAlpha;     ///< 最小アルファ
	float flickerMaxAlpha;     ///< 最大アルファ

	// Block 23 (16 bytes)
	uint32_t flickerRandomPhase; ///< ランダムフェーズフラグ
	uint32_t flickerUseNoise;  ///< ノイズベース点滅フラグ
	float paddingFlicker[2];

	// Block 24 (16 bytes)
	uint32_t hasFaceVelocity;   ///< FaceVelocity有効フラグ
	uint32_t faceVelocityUse2D;  ///< 2Dアライメントフラグ (0=3D, 1=2D)
	float paddingFaceVelocity[2];
};
/**
 * @brief GPUパーティクルシミュレーター
 * 
 * エミッターごとのパーティクルバッファを管理。
 * シェーダー・ルートシグネチャ・PSOは GPUParticlePipeline で共有。
 */
class GPUSimulator
{
public:
	static constexpr uint32_t kDefaultMaxParticles = 65536;
	static constexpr uint32_t kThreadGroupSize = 256;

	GPUSimulator();
	~GPUSimulator();

	/**
	 * @brief 初期化（バッファのみ作成、パイプラインは共有）
	 * @param dxCommon DirectXCommonポインタ
	 * @param srvManager SrvManagerポインタ
	 * @param maxParticles 最大パーティクル数（デフォルト: kDefaultMaxParticles）
	 */
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t maxParticles = kDefaultMaxParticles);

	/**
	 * @brief パーティクルデータをGPUバッファに転送（一括上書き）
	 * @param particles 転送するパーティクルのリスト
	 */
	void UploadParticles(const std::vector<Particle>& particles);

	/**
	 * @brief GPUシミュレーションを実行
	 * @param deltaTime 経過時間（秒）
	 * @param camera カメラマネージャーポインタ
	 * @param modules エミッターが保持するモジュールリスト
	 */
	void Dispatch(float deltaTime, CameraManager* camera, const std::vector<std::unique_ptr<class IModule>>& modules, const Matrix4x4& emitterWorld, uint32_t simulationSpace);

	/**
	 * @brief GPUデータをCPUに読み戻し
	 * @param outParticles 読み戻し先のパーティクルリスト
	 */
	void ReadbackParticles(std::vector<Particle>& outParticles);

	/**
	 * @brief パーティクル数をクリア
	 */
	void ClearParticles();

	/**
	 * @brief 現在のアクティブなパーティクル数を取得（デバッグ用・リードバックキャッシュ）
	 * @return アクティブなパーティクル数
	 */
	uint32_t GetActiveParticleCount() const { return lastActiveCount_; }

	/**
	 * @brief 間接描画引数バッファを初期化
	 * @param type レンダラーの種類
	 * @param elementCount スプライトなら頂点数(4)、メッシュならインデックス数
	 */
	void InitializeIndirectArgs(RendererType type, uint32_t elementCount);

	/**
	 * @brief エミッター位置を設定
	 * @param position エミッター位置
	 */
	void SetEmitterPosition(const Vector3& position) { emitterPosition_ = position; }

	/**
	 * @brief 重力を設定
	 * @param gravity 重力加速度
	 */
	void SetGravity(const Vector3& gravity) { gravity_ = gravity; }

	/**
	 * @brief ビルボード有効フラグを設定
	 * @param isBillboard ビルボード有効フラグ
	 */
	void SetIsBillboard(bool isBillboard) { isBillboard_ = isBillboard; }

	/**
	 * @brief パーティクルバッファのSRVインデックスを取得
	 * @return SRVインデックス
	 */
	uint32_t GetParticleSRVIndex() const { return particleSrvIndex_[dbIndex_]; }

	/**
	 * @brief パーティクルバッファのUAVインデックスを取得
	 * @return UAVインデックス
	 */
	uint32_t GetParticleUavIndex() const { return particleUavIndex_[dbIndex_]; }

	/**
	 * @brief レンダリング用バッファのSRVインデックスを取得
	 * @return SRVインデックス
	 */
	uint32_t GetRenderSrvIndex() const { return renderSrvIndex_; }

	/**
	 * @brief 間接描画用引数バッファを取得
	 */
	ID3D12Resource* GetIndirectArgsBuffer() const { return indirectArgsBuffer_.Get(); }

	/**
	 * @brief 現在のパーティクル数を取得
	 * @return パーティクル数
	 */
	uint32_t GetParticleCount() const { return particleCount_; }

	/**
	 * @brief 最大パーティクル数を取得
	 * @return 最大パーティクル数
	 */
	uint32_t GetMaxParticles() const { return maxParticles_; }

	/**
	 * @brief 初期化済みか判定
	 * @return 初期化済みの場合true
	 */
	bool IsInitialized() const { return initialized_; }

private:
	/**
	 * @brief GPU用バッファを作成（パーティクル、定数、レンダリング用）
	 */
	void CreateBuffers();

	/**
	 * @brief 定数バッファを更新
	 * @param deltaTime 経過時間（秒）
	 */
	void UpdateConstantBuffer(uint32_t index, float deltaTime, const std::vector<std::unique_ptr<class IModule>>& modules, const Matrix4x4& emitterWorld, uint32_t simulationSpace);

private:
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	// ダブルバッファリングするパーティクルバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> particleBuffer_[2];
	
	// スポン用バッファとアップロード用バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> spawnBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> spawnUploadBuffer_;

	// 生存数カウント用カウンタバッファ & クリアバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> counterBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> counterClearBuffer_;

	// 間接描画用引数バッファ & アップロードバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> indirectArgsBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> indirectArgsUploadBuffer_;
	D3D12_RESOURCE_STATES indirectArgsBufferState_ = D3D12_RESOURCE_STATE_COMMON;

	// 定数バッファ（エミッターごとに持つ - ダブルバッファ）
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_[2];
	GPUParticleConstants* constantData_[2] = { nullptr, nullptr };

	// SRV/UAVインデックス（未確保=kInvalidSrvIndex）
	uint32_t particleSrvIndex_[2] = { SrvManager::kInvalidSrvIndex, SrvManager::kInvalidSrvIndex };
	uint32_t particleUavIndex_[2] = { SrvManager::kInvalidSrvIndex, SrvManager::kInvalidSrvIndex };
	
	uint32_t spawnSrvIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t spawnUavIndex_ = SrvManager::kInvalidSrvIndex;

	uint32_t counterSrvIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t counterUavIndex_ = SrvManager::kInvalidSrvIndex;

	uint32_t indirectArgsUavIndex_ = SrvManager::kInvalidSrvIndex;

	uint32_t renderSrvIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t renderUavIndex_ = SrvManager::kInvalidSrvIndex;
	Microsoft::WRL::ComPtr<ID3D12Resource> renderBuffer_;

	// 状態
	uint32_t maxParticles_ = kDefaultMaxParticles;
	uint32_t particleCount_ = 0; // 生存数＋新規発生数の合計
	uint32_t spawnCount_ = 0;    // 新規発生数
	uint32_t dbIndex_ = 0;       // ダブルバッファのインデックス (0 or 1)
	float totalTime_ = 0.0f;
	Vector3 emitterPosition_ = {};
	Vector3 gravity_ = { 0, -9.8f, 0 };
	bool isBillboard_ = true;
	bool initialized_ = false;
	
	// リソース状態追跡
	D3D12_RESOURCE_STATES particleBufferState_[2] = { D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COMMON };
	D3D12_RESOURCE_STATES renderBufferState_ = D3D12_RESOURCE_STATE_COMMON;

	// デバッグ用リードバックカウンタ
	Microsoft::WRL::ComPtr<ID3D12Resource> counterReadbackBuffer_;
	uint32_t lastActiveCount_ = 0;
	uint32_t frameCounter_ = 0;
	bool readbackRequested_ = false;
};
