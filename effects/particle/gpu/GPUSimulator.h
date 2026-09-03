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
#include <algorithm>

namespace KCE
{
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
struct GPUParticleConstants
{
	float deltaTime;           ///< 経過時間（秒）
	float totalTime;           ///< 総経過時間（秒）
	uint32_t particleCount;    ///< 現在のアクティブなパーティクル数
	uint32_t maxParticles;     ///< バッファの最大パーティクル数
	float gpuRibbonWidth;
	uint32_t gpuRibbonWidthFade;
	uint32_t gpuRibbonAlphaFade;
	uint32_t gpuRibbonGroupCount;
	float gpuSpawnRate;
	uint32_t gpuBurstCount;
	float gpuBurstInterval;
	float gpuBurstDelay;
	int32_t gpuBurstLoops;
	uint32_t gpuEmitterIsEmitting;
	uint32_t gpuEmitterReset;
	uint32_t paddingEmitterState;
	uint32_t hasGpuEventSource;
	uint32_t gpuEventTrigger;
	float gpuEventProbability;
	uint32_t gpuEventInheritVelocity;
	float gpuEventVelocityScale;
	uint32_t gpuEventInheritColor;
	float gpuSpawnLifetime;
	uint32_t paddingGpuEvent;
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
	uint32_t hasTextureSheet;
	uint32_t textureSheetColumns;
	uint32_t textureSheetRows;
	uint32_t paddingTextureSheet;

	// Pure GPU spawn parameters. CPU transfers only emitter parameters/counts;
	// particle payloads remain resident on the GPU.
	uint32_t pureGpuEnabled;
	uint32_t spawnCount;
	uint32_t spawnSerialBase;
	uint32_t spawnSeed;
	Vector3 initialVelocityMin; float initialLifetimeMin;
	Vector3 initialVelocityMax; float initialLifetimeMax;
	Vector3 initialScaleMin; float paddingPure0;
	Vector3 initialScaleMax; float paddingPure1;
	Vector4 initialColorMin;
	Vector4 initialColorMax;
	uint32_t hasSpawnShape;
	uint32_t spawnShapeType;
	uint32_t spawnLocation;
	uint32_t spawnEmitFromSurface;
	float spawnInnerRadius;
	float spawnOuterRadius;
	float spawnInitialSpeed;
	float spawnArcRadians;
	Vector3 spawnBoxSize; float spawnConeHeight;
	Vector3 spawnLineStart; float paddingShape0;
	Vector3 spawnLineEnd; float paddingShape1;
};
static_assert(sizeof(GPUParticleConstants) % 16 == 0, "GPU constant-buffer layout must remain 16-byte aligned");

enum class GPUParticleEventType : uint32_t { Spawn = 0, Death = 1, Collision = 2 };
struct GPUParticleEvent
{
	Vector3 position; uint32_t type;
	Vector3 velocity; uint32_t particleId;
	Vector4 color;
};
static_assert(sizeof(GPUParticleEvent) == 48);
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
	void DispatchPure(float deltaTime, CameraManager* camera, const std::vector<std::unique_ptr<class IModule>>& modules, const Matrix4x4& emitterWorld, uint32_t simulationSpace, bool emitting, bool resetEmitterState);
	void SetEventSource(GPUSimulator* source, uint32_t trigger, float probability, bool inheritVelocity, float velocityScale, bool inheritColor);
	void ClearEventSource();
	bool SupportsPureGPU(const std::vector<std::unique_ptr<class IModule>>& modules, RendererType rendererType) const;

	/**
	 * @brief GPUデータをCPUに読み戻し
	 * @param outParticles 読み戻し先のパーティクルリスト
	 */

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
	void SetRibbonParameters(bool enabled, float width, bool widthFade, bool alphaFade, uint32_t groupCount = 1)
	{
		ribbonEnabled_ = enabled; ribbonWidth_ = width; ribbonWidthFade_ = widthFade; ribbonAlphaFade_ = alphaFade;
		ribbonGroupCount_ = (std::max)(1u, groupCount);
	}

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
	ID3D12Resource* GetDrawArgumentsBuffer() const { return drawArgumentsBuffer_.Get(); }
	ID3D12Resource* GetRibbonVertexBuffer() const { return ribbonVertexBuffer_.Get(); }
	ID3D12Resource* GetRibbonDrawArgumentsBuffer() const { return ribbonDrawArgumentsBuffer_.Get(); }
	uint32_t GetEventSrvIndex() const { return eventSrvIndex_; }
	uint32_t GetEventCounterSrvIndex() const { return eventCounterSrvIndex_; }
	bool IsPureGPUPath() const { return lastDispatchWasPure_; }

	/**
	 * @brief 現在のパーティクル数を取得
	 * @return パーティクル数
	 */
	uint32_t GetParticleCount() const { return particleCount_; }
	// Pure GPU mode deliberately never reads the alive counter back to the CPU.
	// Completion is conservatively estimated from the last spawn time and the
	// greatest configured lifetime; rendering uses GPU indirect arguments.
	bool MayHaveLiveParticles() const { return pureGpuDispatchEver_ && (totalTime_ - lastSpawnTime_) < maxSpawnLifetime_; }

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
	void ReleaseDescriptors();

	/**
	 * @brief 定数バッファを更新
	 * @param deltaTime 経過時間（秒）
	 */
	void UpdateConstantBuffer(float deltaTime, const std::vector<std::unique_ptr<class IModule>>& modules, const Matrix4x4& emitterWorld, uint32_t simulationSpace);
	void UpdateModuleProgram(const std::vector<std::unique_ptr<class IModule>>& modules);

private:
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	// パーティクルバッファ（エミッターごとに持つ）
	Microsoft::WRL::ComPtr<ID3D12Resource> particleBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> particleUploadBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> spawnCounterBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> spawnCounterResetBuffer_;
	uint32_t spawnCounterUavIndex_ = SrvManager::kInvalidSrvIndex;
	Microsoft::WRL::ComPtr<ID3D12Resource> emitterStateBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> emitterStateResetBuffer_;
	uint32_t emitterStateUavIndex_ = SrvManager::kInvalidSrvIndex;
	D3D12_RESOURCE_STATES emitterStateBufferState_ = D3D12_RESOURCE_STATE_COMMON;
	Microsoft::WRL::ComPtr<ID3D12Resource> eventBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> eventCounterBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> eventCounterResetBuffer_;
	uint32_t eventSrvIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t eventUavIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t eventCounterSrvIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t eventCounterUavIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t nullEventSrvIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t nullEventCounterSrvIndex_ = SrvManager::kInvalidSrvIndex;
	D3D12_RESOURCE_STATES eventBufferState_ = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_STATES eventCounterState_ = D3D12_RESOURCE_STATE_COMMON;
	Microsoft::WRL::ComPtr<ID3D12Resource> ribbonPrefixBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> ribbonGroupCountBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> ribbonGroupOffsetBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> ribbonVertexBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> ribbonDrawArgumentsBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> ribbonSortBuffer_;
	uint32_t ribbonPrefixUavIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t ribbonGroupCountUavIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t ribbonGroupOffsetUavIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t ribbonVertexUavIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t ribbonDrawArgumentsUavIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t ribbonSortUavIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t ribbonSortCapacity_ = 0;
	D3D12_RESOURCE_STATES ribbonVertexState_ = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_STATES ribbonDrawArgumentsState_ = D3D12_RESOURCE_STATE_COMMON;

	// 定数バッファ（エミッターごとに持つ）
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	GPUParticleConstants* constantData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> moduleProgramBuffer_;
	uint8_t* moduleProgramData_ = nullptr;
	uint32_t moduleProgramSrvIndex_ = SrvManager::kInvalidSrvIndex;
	Microsoft::WRL::ComPtr<ID3D12Resource> moduleLutBuffer_;
	Vector4* moduleLutData_ = nullptr;
	uint32_t moduleLutSrvIndex_ = SrvManager::kInvalidSrvIndex;

	// SRV/UAVインデックス（未確保=kInvalidSrvIndex）
	uint32_t particleSrvIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t particleUavIndex_ = SrvManager::kInvalidSrvIndex;
	
	uint32_t renderSrvIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t renderUavIndex_ = SrvManager::kInvalidSrvIndex;
	Microsoft::WRL::ComPtr<ID3D12Resource> renderBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> drawArgumentsBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> drawArgumentsResetBuffer_;
	uint32_t drawArgumentsUavIndex_ = SrvManager::kInvalidSrvIndex;
	D3D12_RESOURCE_STATES drawArgumentsState_ = D3D12_RESOURCE_STATE_COMMON;

	// 状態
	uint32_t maxParticles_ = kDefaultMaxParticles;
	uint32_t particleCount_ = 0;
	float totalTime_ = 0.0f;
	bool pureGpuDispatch_ = false;
	bool pureGpuEmitting_ = false;
	bool pureGpuResetEmitterState_ = false;
	GPUSimulator* eventSource_ = nullptr;
	uint32_t eventTrigger_ = 1;
	float eventProbability_ = 1.0f;
	bool eventInheritVelocity_ = false;
	float eventVelocityScale_ = 1.0f;
	bool eventInheritColor_ = false;
	bool ribbonEnabled_ = false;
	float ribbonWidth_ = 0.5f;
	bool ribbonWidthFade_ = true;
	bool ribbonAlphaFade_ = true;
	uint32_t ribbonGroupCount_ = 1;
	bool gpuPoolInitialized_ = false;
	bool pureGpuDispatchEver_ = false;
	bool lastDispatchWasPure_ = false;
	float lastSpawnTime_ = -1000000.0f;
	float maxSpawnLifetime_ = 0.0f;
	Vector3 emitterPosition_ = {};
	Vector3 gravity_ = { 0, -9.8f, 0 };
	bool isBillboard_ = true;
	bool initialized_ = false;
	
	// リソース状態追跡
	D3D12_RESOURCE_STATES particleBufferState_ = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_STATES renderBufferState_ = D3D12_RESOURCE_STATE_COMMON;
};
} // namespace KCE
