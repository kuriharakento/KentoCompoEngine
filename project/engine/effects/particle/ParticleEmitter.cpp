#include "ParticleEmitter.h"
#include "effects/particle/module/IModule.h"
#include "effects/particle/module/spawn/SubEmitterModule.h"
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
	// Transform追従
	if (followTarget_)
	{
		position_ = followTarget_->translate + followOffset_;
	}
	// 同じエフェクト内の別エミッター追従（ParticleEffectが位置を設定）
	else if (followingEmitter_)
	{
		position_ = followEmitterPosition_ + followOffset_;
	}

	if (simulationMode_ == SimulationMode::CPU)
	{
		UpdateCPU(deltaTime);
	}
	else
	{
		UpdateGPU(deltaTime, camera);
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
		if (simulationMode_ == SimulationMode::GPU && gpuSimulator_)
		{
			renderer_->SetGPUMode(true, gpuSimulator_->GetRenderSrvIndex(), gpuSimulator_->GetParticleCount());
		}
		else
		{
			renderer_->SetGPUMode(false, 0, 0);
		}
		renderer_->Draw(dxCommon, srvManager);
	}
}

void ParticleEmitter::AddModule(std::unique_ptr<IModule> module)
{
	modules_.push_back(std::move(module));
	modulesSorted_ = false; // 再ソートが必要
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
		gpuSimulator_->Initialize(pm->GetDxCommon(), pm->GetSrvManager(), maxParticles_);
	}
}

void ParticleEmitter::SpawnParticle(const Particle& particle)
{
	if (simulationMode_ == SimulationMode::GPU && gpuSimulator_)
	{
		std::vector<Particle> newParticles = { particle };
		gpuSimulator_->SpawnParticles(newParticles);
	}
	else
	{
		if (particles_.size() < maxParticles_)
		{
			Particle p = particle;
			p.id = nextParticleId_++;
			p.SetAlive(true);
			particles_.push_back(p);
		}
	}
}

void ParticleEmitter::RemoveModule(size_t index)
{
	if (index < modules_.size())
	{
		modules_.erase(modules_.begin() + static_cast<ptrdiff_t>(index));
	}
}

void ParticleEmitter::MoveModuleUp(size_t index)
{
	if (index > 0 && index < modules_.size())
	{
		std::swap(modules_[index], modules_[index - 1]);
		modulesSorted_ = false;
	}
}

void ParticleEmitter::MoveModuleDown(size_t index)
{
	if (index < modules_.size() - 1)
	{
		std::swap(modules_[index], modules_[index + 1]);
		modulesSorted_ = false;
	}
}

void ParticleEmitter::SortModulesByPriority()
{
	if (modulesSorted_) return;

	std::stable_sort(modules_.begin(), modules_.end(),
		[](const std::unique_ptr<IModule>& a, const std::unique_ptr<IModule>& b)
		{
			// まずフェーズでソート
			if (a->GetPhase() != b->GetPhase())
			{
				return static_cast<int>(a->GetPhase()) < static_cast<int>(b->GetPhase());
			}
			// 同じフェーズ内では優先度でソート
			return a->GetPriority() < b->GetPriority();
		});

	modulesSorted_ = true;
}

void ParticleEmitter::UpdateCPU(float deltaTime)
{
	// モジュールをソート（必要な場合のみ）
	SortModulesByPriority();

	ParticleContext context;
	context.particles = &particles_;
	context.deltaTime = deltaTime;
	context.emitterPosition = position_;
	context.followTarget = followTarget_;
	context.spawnCount = 0;

	// Spawnフェーズのモジュールを実行（有効時のみ）
	if (enabled_)
	{
		ExecuteSpawnModules(context);
	}

	// Updateフェーズのモジュールを実行
	ExecuteUpdateModules(context);

	// 基本的な物理更新
	for (auto& particle : particles_)
	{
		if (particle.IsAlive())
		{
			particle.age += deltaTime;
			particle.position += particle.velocity * deltaTime;

			// 寿命チェック
			if (particle.age >= particle.lifetime)
			{
				particle.SetAlive(false);
			}
		}
	}

	// 死んだパーティクルを削除
	RemoveDeadParticles();
}

void ParticleEmitter::UpdateGPU(float deltaTime, CameraManager* camera)
{
	if (gpuSimulator_)
	{
		gpuSimulator_->SetEmitterPosition(position_);
		gpuSimulator_->Dispatch(deltaTime, camera);
		
		// GPUからパーティクルデータを読み戻してCPU側に反映
		// これによりレンダラーがパーティクルを描画できる
		gpuSimulator_->ReadbackParticles(particles_);
	}
	else
	{
		UpdateCPU(deltaTime);
	}
}

void ParticleEmitter::RemoveDeadParticles()
{
	// SubEmitter OnDeath trigger
	SubEmitterModule* subEmitter = nullptr;
	for (auto& module : modules_)
	{
		if ((subEmitter = dynamic_cast<SubEmitterModule*>(module.get())))
		{
			break;
		}
	}

	if (subEmitter)
	{
		auto* manager = ParticleManager::GetInstance();
		for (const auto& particle : particles_)
		{
			if (!particle.IsAlive())
			{
				subEmitter->TriggerSubEmitters(SubEmitterTrigger::OnDeath, particle, manager);
			}
		}
	}

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
		if (module->GetPhase() == ModulePhase::Spawn)
		{
			module->Execute(context);
		}
	}
}

void ParticleEmitter::ExecuteUpdateModules(ParticleContext& context)
{
	for (auto& module : modules_)
	{
		if (module->GetPhase() == ModulePhase::Update)
		{
			module->Execute(context);
		}
	}
}
