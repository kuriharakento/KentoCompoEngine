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


/**
 * @brief GPUパーティクル定数バッファデータ構造体
 * 
 * GPUシミュレーション用の定数パラメータを格納。
 * シェーダーに渡される。
 */
struct GPUParticleConstants
{
	float deltaTime;           ///< 経過時間（秒）
	float totalTime;           ///< 総経過時間（秒）
	uint32_t particleCount;    ///< 現在のアクティブなパーティクル数
	uint32_t maxParticles;     ///< バッファの最大パーティクル数
	Vector3 emitterPosition;   ///< エミッター位置
	uint32_t isBillboard;      ///< ビルボード有効フラグ（0=無効, 1=有効）
	Vector3 gravity;           ///< 重力加速度
	uint32_t simulationSpace;  ///< シミュレーション空間（0=World, 1=Local）
	Matrix4x4 emitterWorld;    ///< エミッターのワールド行列

	//===== 追加モジュールパラメータ (アプローチB) =====//
	uint32_t hasDrag;          ///< Drag有効フラグ
	float dragMin;             ///< 最小Drag
	float dragMax;             ///< 最大Drag
	float paddingDrag;
	
	uint32_t hasColorFade;     ///< ColorFade有効フラグ
	uint32_t colorFadeUseInitial; ///< 初期カラー使用フラグ
	uint32_t colorFadeEasing;  ///< イージングタイプ (EasingType)
	float paddingCF;
	Vector4 colorFadeStart;    ///< 開始カラー
	Vector4 colorFadeEnd;      ///< 終了カラー
	
	uint32_t hasScaleOL;       ///< ScaleOverLifetime有効フラグ
	uint32_t scaleOLEasing;    ///< イージングタイプ (EasingType)
	float paddingScaleOL[2];
	Vector3 scaleOLStart;      ///< 開始スケール
	float paddingS1;
	Vector3 scaleOLEnd;        ///< 終了スケール
	float paddingS2;

	// Noise
	uint32_t hasNoise;         ///< Noise有効フラグ
	float noiseStrength;       ///< Noise強度
	float noiseFrequency;      ///< Noise周波数
	float paddingNoise;

	// RotationOverLifetime
	uint32_t hasRotationOL;    ///< RotationOverLifetime有効フラグ
	float rotOLStartSpeed;     ///< 開始回転速度
	float rotOLEndSpeed;       ///< 終了回転速度
	uint32_t rotOLEasing;      ///< イージングタイプ (EasingType)

	// AlphaFade
	uint32_t hasAlphaFade;     ///< AlphaFade有効フラグ
	float alphaFadeStart;      ///< 開始アルファ
	float alphaFadeEnd;        ///< 終了アルファ
	uint32_t alphaFadeEaseIn;  ///< EaseInフラグ
	uint32_t alphaFadeEaseOut; ///< EaseOutフラグ
	float paddingAlpha[3];

	// VelocityOverLifetime
	uint32_t hasVelocityOL;    ///< VelocityOverLifetime有効フラグ
	float velocityOLStart;     ///< 開始乗数
	float velocityOLEnd;       ///< 終了乗数
	float paddingVelocityOL;

	// StretchByVelocity
	uint32_t hasStretchByVelocity; ///< StretchByVelocity有効フラグ
	float stretchFactor;       ///< ストレッチ係数
	float minStretch;          ///< 最小ストレッチ
	float maxStretch;          ///< 最大ストレッチ
	uint32_t stretchPreserveVolume; ///< 体積維持フラグ (0=無効, 1=有効)
	float paddingStretch[3];

	// Flicker
	uint32_t hasFlicker;       ///< Flicker有効フラグ
	float flickerFrequency;    ///< 周波数
	float flickerMinAlpha;     ///< 最小アルファ
	float flickerMaxAlpha;     ///< 最大アルファ
	uint32_t flickerRandomPhase; ///< ランダムフェーズフラグ
	uint32_t flickerUseNoise;  ///< ノイズベース点滅フラグ
	float paddingFlicker[2];

	// FaceVelocity
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
	uint32_t GetParticleSRVIndex() const { return particleSrvIndex_; }

	/**
	 * @brief パーティクルバッファのUAVインデックスを取得
	 * @return UAVインデックス
	 */
	uint32_t GetParticleUavIndex() const { return particleUavIndex_; }

	/**
	 * @brief レンダリング用バッファのSRVインデックスを取得
	 * @return SRVインデックス
	 */
	uint32_t GetRenderSrvIndex() const { return renderSrvIndex_; }

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
	void UpdateConstantBuffer(float deltaTime, const std::vector<std::unique_ptr<class IModule>>& modules, const Matrix4x4& emitterWorld, uint32_t simulationSpace);

private:
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	// パーティクルバッファ（エミッターごとに持つ）
	Microsoft::WRL::ComPtr<ID3D12Resource> particleBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> particleUploadBuffer_;
	static constexpr uint32_t kNumReadbackBuffers = 2;
	Microsoft::WRL::ComPtr<ID3D12Resource> particleReadbackBuffer_[kNumReadbackBuffers];
	uint32_t readbackFrameIndex_ = 0;

	// 定数バッファ（エミッターごとに持つ）
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	GPUParticleConstants* constantData_ = nullptr;

	// SRV/UAVインデックス（未確保=kInvalidSrvIndex）
	uint32_t particleSrvIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t particleUavIndex_ = SrvManager::kInvalidSrvIndex;
	
	uint32_t renderSrvIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t renderUavIndex_ = SrvManager::kInvalidSrvIndex;
	Microsoft::WRL::ComPtr<ID3D12Resource> renderBuffer_;

	// 状態
	uint32_t maxParticles_ = kDefaultMaxParticles;
	uint32_t particleCount_ = 0;
	float totalTime_ = 0.0f;
	Vector3 emitterPosition_ = {};
	Vector3 gravity_ = { 0, -9.8f, 0 };
	bool isBillboard_ = true;
	bool initialized_ = false;
	
	// リソース状態追跡
	D3D12_RESOURCE_STATES particleBufferState_ = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_STATES renderBufferState_ = D3D12_RESOURCE_STATE_COMMON;
};
