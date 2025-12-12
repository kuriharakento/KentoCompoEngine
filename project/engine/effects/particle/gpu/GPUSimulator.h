#pragma once
/**
 * @file GPUSimulator.h
 * @brief GPUパーティクルシミュレーター
 * 
 * コンピュートシェーダーによるパーティクルシミュレーション。
 * エミッターごとのバッファを管理し、GPUで並列計算。
 */
#include "effects/particle/Particle.h"
#include "math/Vector3.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <vector>

class DirectXCommon;
class SrvManager;
class CameraManager;


/**
 * @brief GPUパーティクル定数バッファデータ構造体
 */
struct GPUParticleConstants
{
	float deltaTime;      // 経過時間
	float totalTime;      // 総時間
	uint32_t particleCount; // 現在のパーティクル数
	uint32_t maxParticles;  // 最大パーティクル数
	Vector3 emitterPosition; // エミッター位置
	float padding1;
	Vector3 gravity;        // 重力
	uint32_t isBillboard;   // ビルボード有効フラグ
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
	 * @param dxCommon DirectXCommonインスタンス
	 * @param srvManager SRVマネージャー
	 * @param maxParticles 最大パーティクル数（デフォルト: kDefaultMaxParticles）
	 */
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t maxParticles = kDefaultMaxParticles);

	/**
	 * @brief パーティクルを生成
	 * @param newParticles 生成する新規パーティクルの配列
	 */
	void SpawnParticles(const std::vector<Particle>& newParticles);

	/**
	 * @brief GPU上でシミュレーションを実行
	 * @param deltaTime デルタタイム（秒）
	 * @param camera カメラマネージャー
	 */
	void Dispatch(float deltaTime, CameraManager* camera);

	/**
	 * @brief GPUデータをCPUに読み戻し
	 * @param outParticles 出力先パーティクル配列
	 */
	void ReadbackParticles(std::vector<Particle>& outParticles);

	/**
	 * @brief エミッター位置を設定
	 * @param position 位置ベクトル
	 */
	void SetEmitterPosition(const Vector3& position) { emitterPosition_ = position; }
	
	/**
	 * @brief 重力を設定
	 * @param gravity 重力ベクトル
	 */
	void SetGravity(const Vector3& gravity) { gravity_ = gravity; }
	
	/**
	 * @brief ビルボード有効フラグを設定
	 * @param isBillboard ビルボードを有効にする場合true
	 */
	void SetIsBillboard(bool isBillboard) { isBillboard_ = isBillboard; }

	/**
	 * @brief パーティクルバッファのSRVインデックスを取得
	 * @return SRVインデックス
	 */
	uint32_t GetParticleSRVIndex() const { return particleSrvIndex_; }
	
	/**
	 * @brief レンダリングバッファのSRVインデックスを取得
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
	 * @brief 初期化済みかどうかを判定
	 * @return 初期化済みの場合true
	 */
	bool IsInitialized() const { return initialized_; }

private:
	/**
	 * @brief バッファを作成
	 */
	void CreateBuffers();
	
	/**
	 * @brief 定数バッファを更新
	 * @param deltaTime デルタタイム（秒）
	 */
	void UpdateConstantBuffer(float deltaTime);

private:
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	// パーティクルバッファ（エミッターごとに持つ）
	Microsoft::WRL::ComPtr<ID3D12Resource> particleBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> particleUploadBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> particleReadbackBuffer_;

	// 定数バッファ（エミッターごとに持つ）
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	GPUParticleConstants* constantData_ = nullptr;

	// SRV/UAVインデックス
	uint32_t particleSrvIndex_ = 0;
	uint32_t particleUavIndex_ = 0;
	
	uint32_t renderSrvIndex_ = 0;
	uint32_t renderUavIndex_ = 0;
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
};
