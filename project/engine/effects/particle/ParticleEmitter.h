#pragma once
#include <memory>
#include <vector>
#include <string>
#include "Particle.h"
#include "ParticleTypes.h"

// 前方宣言
class IModule;
class IRenderer;
class CameraManager;
class DirectXCommon;
class SrvManager;
class Transform;
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

	const std::string& GetName() const { return name_; }

	std::vector<Particle>& GetParticles() { return particles_; }
	const std::vector<Particle>& GetParticles() const { return particles_; }

	void SpawnParticle(const Particle& particle);
	GPUSimulator* GetGPUSimulator() const { return gpuSimulator_.get(); }

private:
	void UpdateCPU(float deltaTime);
	void UpdateGPU(float deltaTime);
	void RemoveDeadParticles();
	void ExecuteSpawnModules(ParticleContext& context);
	void ExecuteUpdateModules(ParticleContext& context);

private:
	std::string name_;
	std::vector<Particle> particles_;
	std::vector<std::unique_ptr<IModule>> modules_;
	std::unique_ptr<IRenderer> renderer_;
	std::unique_ptr<GPUSimulator> gpuSimulator_;

	uint32_t maxParticles_ = 1000;
	SimulationMode simulationMode_ = SimulationMode::CPU;
	SimulationSpace simulationSpace_ = SimulationSpace::World;

	Vector3 position_ = {};
	Transform* followTarget_ = nullptr;
	uint32_t nextParticleId_ = 0;
};
