#pragma once
/**
 * @file ParticleEmitter.h
 * @brief パーティクルエミッター
 * 
 * 単一のパーティクル発生源。モジュールによる挙動制御と
 * レンダラーによる描画を管理。CPU/GPUシミュレーション対応。
 */
#include <memory>
#include <vector>
#include <string>
#include "Particle.h"
#include "ParticleTypes.h"
#include <base/GraphicsTypes.h>

// 前方宣言
class IModule;
class IRenderer;
class CameraManager;
class DirectXCommon;
class SrvManager;
class GPUSimulator;

/**
 * @brief パーティクル実行コンテキスト
 */
struct ParticleContext
{
	std::vector<Particle>* particles = nullptr;
	float deltaTime = 0.0f;
	Vector3 emitterPosition = {};
	Transform* followTarget = nullptr;
	uint32_t spawnCount = 0;
};

/**
 * @brief パーティクルエミッター
 */
class ParticleEmitter
{
public:
	ParticleEmitter();
	~ParticleEmitter();

	void Initialize(const std::string& name);
	void Update(float deltaTime, CameraManager* camera);
	void Draw(DirectXCommon* dxCommon, SrvManager* srvManager);

	void AddModule(std::unique_ptr<IModule> module);
	void SetRenderer(std::unique_ptr<IRenderer> renderer);
	IRenderer* GetRenderer() const { return renderer_.get(); }

	void SetMaxParticles(uint32_t max) { maxParticles_ = max; }
	uint32_t GetMaxParticles() const { return maxParticles_; }

	void SetSimulationMode(SimulationMode mode);
	SimulationMode GetSimulationMode() const { return simulationMode_; }

	void SetSimulationSpace(SimulationSpace space) { simulationSpace_ = space; }
	SimulationSpace GetSimulationSpace() const { return simulationSpace_; }

	void SetPosition(const Vector3& pos) { position_ = pos; }
	const Vector3& GetPosition() const { return position_; }

	void SetFollowTarget(Transform* target) { followTarget_ = target; }
	Transform* GetFollowTarget() const { return followTarget_; }

	void SetFollowOffset(const Vector3& offset) { followOffset_ = offset; }
	const Vector3& GetFollowOffset() const { return followOffset_; }

	// 同じエフェクト内の別エミッターを追従 (-1 = 追従しない)
	void SetFollowEmitterIndex(int index) { followEmitterIndex_ = index; }
	int GetFollowEmitterIndex() const { return followEmitterIndex_; }
	
	// 追従対象のエミッター位置を設定（ParticleEffectから呼ばれる）
	void SetFollowEmitterPosition(const Vector3& pos) { followEmitterPosition_ = pos; followingEmitter_ = true; }
	void ClearFollowEmitterPosition() { followingEmitter_ = false; }

	const std::string& GetName() const { return name_; }

	// Play/Stop制御
	void SetEnabled(bool enabled) { enabled_ = enabled; }
	bool IsEnabled() const { return enabled_; }
	void Play();
	void Stop();
	void Pause();
	void Resume();
	void Reset();
	/**
	 * @brief エミッター状態を再スタート（パーティクルは消さない）
	 *
	 * isEmitting_などのライフサイクル状態だけリセットして再度生成を開始する。
	 * 既存パーティクルを残したまま追加生成したい場合に使う。
	 */
	void Restart();
	void ClearParticles() { particles_.clear(); }

	//===== ライフサイクル設定 =====//

	// Duration: エミッター持続時間（0 = 無限）
	void SetDuration(float duration) { duration_ = duration; }
	float GetDuration() const { return duration_; }

	// StartDelay: 開始遅延
	void SetStartDelay(float delay) { startDelay_ = delay; }
	float GetStartDelay() const { return startDelay_; }

	// LoopBehavior: ループ挙動
	void SetLoopBehavior(LoopBehavior behavior) { loopBehavior_ = behavior; }
	LoopBehavior GetLoopBehavior() const { return loopBehavior_; }

	// LoopCount: Multiple時のループ回数
	void SetLoopCount(int count) { loopCount_ = count; }
	int GetLoopCount() const { return loopCount_; }

	// InactiveResponse: 停止時の挙動
	void SetInactiveResponse(InactiveResponse response) { inactiveResponse_ = response; }
	InactiveResponse GetInactiveResponse() const { return inactiveResponse_; }

	// 状態取得
	float GetEmitterAge() const { return emitterAge_; }
	bool IsEmitting() const { return isEmitting_; }
	bool IsComplete() const;

	// 移動時のみパーティクル生成
	void SetSpawnOnlyWhenMoving(bool enable) { spawnOnlyWhenMoving_ = enable; }
	bool GetSpawnOnlyWhenMoving() const { return spawnOnlyWhenMoving_; }
	void SetMinMoveDistance(float distance) { minMoveDistance_ = distance; }
	float GetMinMoveDistance() const { return minMoveDistance_; }

	std::vector<Particle>& GetParticles() { return particles_; }
	const std::vector<Particle>& GetParticles() const { return particles_; }

	void SpawnParticle(const Particle& particle);

	// モジュールアクセス
	size_t GetModuleCount() const { return modules_.size(); }
	IModule* GetModule(size_t index) { return index < modules_.size() ? modules_[index].get() : nullptr; }
	const IModule* GetModule(size_t index) const { return index < modules_.size() ? modules_[index].get() : nullptr; }

	/**
	 * @brief モジュールを型で取得
	 * @tparam T 取得するモジュールの型（IModuleの派生クラス）
	 * @return 見つかったモジュール。なければnullptr
	 */
	template<typename T>
	T* GetModule()
	{
		for (auto& module : modules_)
		{
			if (auto* casted = dynamic_cast<T*>(module.get()))
			{
				return casted;
			}
		}
		return nullptr;
	}

	template<typename T>
	const T* GetModule() const
	{
		for (const auto& module : modules_)
		{
			if (auto* casted = dynamic_cast<const T*>(module.get()))
			{
				return casted;
			}
		}
		return nullptr;
	}

	/**
	 * @brief モジュールを名前で取得
	 * @param name モジュール名（GetName()で返る名前）
	 * @return 見つかったモジュール。なければnullptr
	 */
	IModule* GetModuleByName(const std::string& name);
	const IModule* GetModuleByName(const std::string& name) const;

	void RemoveModule(size_t index);
	void MoveModuleUp(size_t index);
	void MoveModuleDown(size_t index);

	GPUSimulator* GetGPUSimulator() const { return gpuSimulator_.get(); }

private:
	void UpdateCPU(float deltaTime);
	void UpdateGPU(float deltaTime, CameraManager* camera);
	void RemoveDeadParticles();
	void ExecuteSpawnModules(ParticleContext& context);
	void ExecuteUpdateModules(ParticleContext& context);
	void SortModulesByPriority();

private:
	//===== 基本情報 =====//
	std::string name_;                              ///< エミッター名
	std::vector<Particle> particles_;               ///< アクティブなパーティクルリスト
	std::vector<std::unique_ptr<IModule>> modules_; ///< モジュールリスト（優先度順）
	std::unique_ptr<IRenderer> renderer_;           ///< レンダラー（描画担当）
	std::unique_ptr<GPUSimulator> gpuSimulator_;    ///< GPUシミュレーター（GPUモード時に使用）

	//===== シミュレーション設定 =====//
	uint32_t maxParticles_ = 1000;                  ///< 最大パーティクル数
	SimulationMode simulationMode_ = SimulationMode::CPU;   ///< シミュレーションモード（CPU/GPU）
	SimulationSpace simulationSpace_ = SimulationSpace::World; ///< シミュレーション空間（World/Local）

	//===== 位置・追従 =====//
	Vector3 position_ = {};                         ///< エミッター位置
	Vector3 followOffset_ = {};                     ///< 追従時のオフセット
	Transform* followTarget_ = nullptr;             ///< 追従対象のTransform（外部オブジェクト追従）
	int followEmitterIndex_ = -1;                   ///< 同じエフェクト内の別エミッターインデックス（-1:追従なし）
	Vector3 followEmitterPosition_ = {};            ///< 追従対象エミッターの位置（ParticleEffectが設定）
	bool followingEmitter_ = false;                 ///< エミッター追従中フラグ

	//===== 内部状態 =====//
	uint32_t nextParticleId_ = 0;                   ///< 次に生成するパーティクルのID
	bool modulesSorted_ = false;                    ///< モジュールが優先度順にソート済みか
	bool enabled_ = true;                           ///< エミッター有効フラグ

	//===== ライフサイクル設定 =====//
	float duration_ = 0.0f;                         ///< 持続時間（0 = 無限）
	float startDelay_ = 0.0f;                       ///< 開始遅延
	LoopBehavior loopBehavior_ = LoopBehavior::Infinite;  ///< ループ挙動
	int loopCount_ = 1;                             ///< Multiple時のループ回数
	InactiveResponse inactiveResponse_ = InactiveResponse::Complete;  ///< 停止時挙動

	//===== ライフサイクル状態 =====//
	float emitterAge_ = 0.0f;                       ///< エミッター経過時間
	int currentLoopCount_ = 0;                      ///< 現在のループ回数
	bool isEmitting_ = true;                        ///< パーティクル生成中か
	bool delayElapsed_ = false;                     ///< 遅延経過済みか
	bool isPaused_ = false;                         ///< 一時停止中か

	//===== 移動検出 =====//
	bool spawnOnlyWhenMoving_ = false;              ///< 移動時のみパーティクル生成するか
	float minMoveDistance_ = 0.05f;                 ///< 生成に必要な最小移動距離
	Vector3 previousPosition_ = {};                 ///< 前フレームの位置
	bool hasPreviousPosition_ = false;              ///< 前フレームの位置が有効か
};
