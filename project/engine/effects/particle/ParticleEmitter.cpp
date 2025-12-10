#include "ParticleEmitter.h"
#include "effects/particle/module/IModule.h"
#include "effects/particle/renderer/IRenderer.h"
#include "effects/particle/gpu/GPUSimulator.h"
#include "effects/particle/ParticleManager.h"
#include <algorithm>

ParticleEmitter::ParticleEmitter() = default;
ParticleEmitter::~ParticleEmitter() = default;

void ParticleEmitter::Initialize(const std::string& name)
{
	name_ = name;
	particles_.reserve(maxParticles_);
}

void ParticleEmitter::Update(float deltaTime, CameraManager* camera)
{
	if (followTarget_)
	{
		// TODO: Transformから位置を取得
	}

	if (simulationMode_ == SimulationMode::CPU)
	{
		UpdateCPU(deltaTime);
	}
	else
	{
		UpdateGPU(deltaTime);
	}

	if (renderer_)
	{
		renderer_->Update(particles_, camera);
	}
}

void ParticleEmitter::Draw(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	if (renderer_)
	{
		renderer_->Draw(dxCommon, srvManager);
	}
}

void ParticleEmitter::AddModule(std::unique_ptr<IModule> module)
{
	modules_.push_back(std::move(module));
}

void ParticleEmitter::SetRenderer(std::unique_ptr<IRenderer> renderer)
{
	renderer_ = std::move(renderer);
}

void ParticleEmitter::SetSimulationMode(SimulationMode mode)
{
	if (simulationMode_ == mode) return;

	simulationMode_ = mode;

	if (mode == SimulationMode::GPU && !gpuSimulator_)
	{
		auto* pm = ParticleManager::GetInstance();
		gpuSimulator_ = std::make_unique<GPUSimulator>();
		gpuSimulator_->Initialize(pm->GetDxCommon(), maxParticles_);
	}
}

void ParticleEmitter::SpawnParticle(const Particle& particle)
{
	if (simulationMode_ == SimulationMode::GPU && gpuSimulator_)
	{
		gpuSimulator_->SpawnParticles(1);
	}
	else
	{
		if (particles_.size() < maxParticles_)
		{
			Particle p = particle;
			p.id = nextParticleId_++;
			particles_.push_back(p);
		}
	}
}

void ParticleEmitter::UpdateCPU(float deltaTime)
{
	ParticleContext context;
	context.particles = &particles_;
	context.deltaTime = deltaTime;
	context.emitterPosition = position_;
	context.followTarget = followTarget_;
	context.spawnCount = 0;

	ExecuteSpawnModules(context);
	ExecuteUpdateModules(context);

	for (auto& particle : particles_)
	{
		particle.age += deltaTime;
		particle.position += particle.velocity * deltaTime;
	}

	RemoveDeadParticles();
}

void ParticleEmitter::UpdateGPU(float deltaTime)
{
	if (gpuSimulator_)
	{
		gpuSimulator_->SetEmitterPosition(position_);
		gpuSimulator_->Dispatch(deltaTime);
	}
	else
	{
		UpdateCPU(deltaTime);
	}
}

void ParticleEmitter::RemoveDeadParticles()
{
	particles_.erase(
		std::remove_if(particles_.begin(), particles_.end(),
			[](const Particle& p) { return !p.IsAlive(); }),
		particles_.end()
	);
}

void ParticleEmitter::ExecuteSpawnModules(ParticleContext& context)
{
	for (auto& module : modules_)
	{
		if (module->GetPhase() == ModulePhase::EmitterSpawn ||
			module->GetPhase() == ModulePhase::EmitterUpdate ||
			module->GetPhase() == ModulePhase::ParticleSpawn)
		{
			module->Execute(context);
		}
	}
}

void ParticleEmitter::ExecuteUpdateModules(ParticleContext& context)
{
	for (auto& module : modules_)
	{
		if (module->GetPhase() == ModulePhase::ParticleUpdate)
		{
			module->Execute(context);
		}
	}
}
