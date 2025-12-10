#pragma once
#include <memory>
#include <vector>
#include <string>
#include "ParticleEmitter.h"

class DirectXCommon;
class SrvManager;
class CameraManager;
class ParticlePipelineManager;

/**
 * @brief パーティクルマネージャー
 */
class ParticleManager
{
public:
	static ParticleManager* GetInstance();

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void Finalize();
	void Update(CameraManager* camera);
	void Draw();

	void AddEmitter(std::unique_ptr<ParticleEmitter> emitter);
	ParticleEmitter* GetEmitter(const std::string& name);
	void Clear();

	DirectXCommon* GetDxCommon() const { return dxCommon_; }
	SrvManager* GetSrvManager() const { return srvManager_; }

private:
	ParticleManager() = default;
	~ParticleManager() = default;
	ParticleManager(const ParticleManager&) = delete;
	ParticleManager& operator=(const ParticleManager&) = delete;

private:
	std::vector<std::unique_ptr<ParticleEmitter>> emitters_;
	std::unique_ptr<ParticlePipelineManager> pipelineManager_;
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
};
