#include "ParticleManager.h"
#include "base/DirectXCommon.h"
#include "manager/system/SrvManager.h"
#include "manager/scene/CameraManager.h"
#include "manager/effect/ParticlePipelineManager.h"
#include "time/TimeManager.h"

ParticleManager* ParticleManager::GetInstance()
{
	static ParticleManager instance;
	return &instance;
}

void ParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	pipelineManager_ = std::make_unique<ParticlePipelineManager>();
	pipelineManager_->Initialize(dxCommon_);
}

void ParticleManager::Finalize()
{
	emitters_.clear();
	pipelineManager_.reset();
}

void ParticleManager::Update(CameraManager* camera)
{
	float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;
	for (auto& emitter : emitters_)
	{
		emitter->Update(deltaTime, camera);
	}
}

void ParticleManager::Draw()
{
	if (emitters_.empty()) return;

	dxCommon_->GetCommandList()->SetGraphicsRootSignature(pipelineManager_->GetRootSignature());
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	dxCommon_->GetCommandList()->SetPipelineState(pipelineManager_->GetPipelineState(BlendMode::Additive));

	for (auto& emitter : emitters_)
	{
		emitter->Draw(dxCommon_, srvManager_);
	}
}

void ParticleManager::AddEmitter(std::unique_ptr<ParticleEmitter> emitter)
{
	emitters_.push_back(std::move(emitter));
}

ParticleEmitter* ParticleManager::GetEmitter(const std::string& name)
{
	for (auto& emitter : emitters_)
	{
		if (emitter->GetName() == name)
		{
			return emitter.get();
		}
	}
	return nullptr;
}

void ParticleManager::Clear()
{
	emitters_.clear();
}
