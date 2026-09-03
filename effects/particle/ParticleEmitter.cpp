#include "ParticleEmitter.h"
#include "effects/particle/module/IModule.h"
#include "effects/particle/module/spawn/SubEmitterModule.h"
#include "effects/particle/module/spawn/SpawnModules.h"
#include "effects/particle/module/spawn/InitialModules.h"
#include "effects/particle/module/spawn/SpawnShapeModules.h"
#include "effects/particle/module/update/UpdateModules.h"
#include "effects/particle/module/update/AdvancedModules.h"
#include "effects/particle/module/update/MotionEffectModules.h"
#include "effects/particle/renderer/IRenderer.h"
#include "effects/particle/renderer/TrailRenderer.h"
#include "effects/particle/gpu/GPUSimulator.h"
#include "effects/particle/ParticleManager.h"
#include "base/DirectXCommon.h"
#include <algorithm>
#include <cmath>
#include "effects/particle/diagnostics/ParticleDiagnostics.h"

namespace KCE
{
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
	ApplyDynamicBindings();

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
		if (simulationMode_ == SimulationMode::GPU && gpuSimulator_ && gpuSimulator_->IsInitialized() && gpuSimulator_->IsPureGPUPath())
		{
			renderer_->SetGPUMode(true, gpuSimulator_->GetRenderSrvIndex(), gpuSimulator_->GetParticleCount(),
				gpuSimulator_->IsPureGPUPath() ? gpuSimulator_->GetDrawArgumentsBuffer() : nullptr);
			const bool gpuRibbon = gpuSimulator_->IsPureGPUPath() && renderer_->GetType() == RendererType::Ribbon;
			renderer_->SetGPURibbonMode(gpuRibbon, gpuSimulator_->GetRibbonVertexBuffer(),
				gpuSimulator_->GetRibbonDrawArgumentsBuffer(), gpuSimulator_->GetMaxParticles() * 6u);
		}
		else
		{
			renderer_->SetGPUMode(false, 0, 0);
			renderer_->SetGPURibbonMode(false, nullptr, nullptr, 0);
		}
		renderer_->Update(particles_, camera);
	}
}

void ParticleEmitter::Draw(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	if (renderer_)
	{
		if (simulationMode_ == SimulationMode::GPU && gpuSimulator_ && gpuSimulator_->IsInitialized() && gpuSimulator_->IsPureGPUPath())
		{
			renderer_->SetGPUMode(true, gpuSimulator_->GetRenderSrvIndex(), gpuSimulator_->GetParticleCount(),
				gpuSimulator_->IsPureGPUPath() ? gpuSimulator_->GetDrawArgumentsBuffer() : nullptr);
			const bool gpuRibbon = gpuSimulator_->IsPureGPUPath() && renderer_->GetType() == RendererType::Ribbon;
			renderer_->SetGPURibbonMode(gpuRibbon, gpuSimulator_->GetRibbonVertexBuffer(),
				gpuSimulator_->GetRibbonDrawArgumentsBuffer(), gpuSimulator_->GetMaxParticles() * 6u);
		}
		else
		{
			renderer_->SetGPUMode(false, 0, 0);
			renderer_->SetGPURibbonMode(false, nullptr, nullptr, 0);
		}
		renderer_->Draw(dxCommon, srvManager);
	}
}

void ParticleEmitter::AddModule(std::unique_ptr<IModule> module)
{
	modules_.push_back(std::move(module));
	modulesSorted_ = false; // 再ソートが必要
	compiledEmitterDirty_ = true;
}

void ParticleEmitter::SetMaxParticles(uint32_t max)
{
	constexpr uint32_t kMaximumParticleCapacity = 1000000u;
	max = (std::clamp)(max, 1u, kMaximumParticleCapacity);
	if (maxParticles_ == max) return;
	const uint32_t previousCapacity = maxParticles_;
	maxParticles_ = max;

	// GPUシミュレーターが存在する場合は再初期化（再確保）する
	if (gpuSimulator_)
	{
		auto* pm = ParticleManager::GetInstance();
		auto replacement = std::make_unique<GPUSimulator>();
		replacement->Initialize(pm->GetDxCommon(), pm->GetSrvManager(), maxParticles_);
		if (!replacement->IsInitialized())
		{
			maxParticles_ = previousCapacity;
			return;
		}
		pm->AddSimulatorToTrashBin(std::move(gpuSimulator_));
		gpuSimulator_ = std::move(replacement);
	}
	if (particles_.size() > maxParticles_) particles_.resize(maxParticles_);
	particles_.reserve(maxParticles_);
}

void ParticleEmitter::ApplyDynamicBindings()
{
	if (dynamicBindings_.empty()) return;
	const float normalizedTime = duration_ > 0.0f ? (std::clamp)(emitterAge_ / duration_, 0.0f, 1.0f) : emitterAge_ - std::floor(emitterAge_);
	const uint32_t seed = nextParticleId_ ^ static_cast<uint32_t>(emitterAge_ * 1000.0f);
	auto value = [&](const char* module, const char* parameter) -> const ModuleParameterValue*
	{
		const auto* binding = FindDynamicBinding(module, parameter);
		if (!binding) return nullptr;
		static thread_local ModuleParameterValue evaluated;
		evaluated = binding->Evaluate(normalizedTime, seed, &parameterStore_);
		return &evaluated;
	};
	auto getFloat = [&](const char* module, const char* parameter, float fallback)
	{
		const auto* resolved = value(module, parameter); const auto* typed = resolved ? std::get_if<float>(resolved) : nullptr; return typed ? *typed : fallback;
	};
	auto getV3 = [&](const char* module, const char* parameter, Vector3 fallback)
	{
		const auto* resolved = value(module, parameter); const auto* typed = resolved ? std::get_if<Vector3>(resolved) : nullptr; return typed ? *typed : fallback;
	};
	auto getV4 = [&](const char* module, const char* parameter, Vector4 fallback)
	{
		const auto* resolved = value(module, parameter); const auto* typed = resolved ? std::get_if<Vector4>(resolved) : nullptr; return typed ? *typed : fallback;
	};
	for (auto& module : modules_)
	{
		if (auto* m = dynamic_cast<SpawnRateModule*>(module.get())) m->SetRate(getFloat("SpawnRate", "rate", m->GetRate()));
		else if (auto* m = dynamic_cast<SpawnShapeModule*>(module.get()))
		{
			m->SetRadius(getFloat("SpawnShape", "innerRadius", m->GetInnerRadius()), getFloat("SpawnShape", "outerRadius", m->GetOuterRadius()));
			m->SetBoxSize(getV3("SpawnShape", "boxSize", m->GetBoxSize()));
			m->SetConeHeight(getFloat("SpawnShape", "coneHeight", m->GetConeHeight()));
			m->SetLine(getV3("SpawnShape", "lineStart", m->GetLineStart()), getV3("SpawnShape", "lineEnd", m->GetLineEnd()));
			m->SetInitialSpeed(getFloat("SpawnShape", "initialSpeed", m->GetInitialSpeed()));
			m->SetArcAngle(getFloat("SpawnShape", "arcAngle", m->GetArcAngle()));
		}
		else if (auto* m = dynamic_cast<InitialPositionModule*>(module.get())) m->SetOffsetRange(getV3("InitialPosition", "min", m->GetMinOffset()), getV3("InitialPosition", "max", m->GetMaxOffset()));
		else if (auto* m = dynamic_cast<InitialVelocityModule*>(module.get())) m->SetVelocityRange(getV3("InitialVelocity", "min", m->GetMinVelocity()), getV3("InitialVelocity", "max", m->GetMaxVelocity()));
		else if (auto* m = dynamic_cast<InitialLifetimeModule*>(module.get())) m->SetLifetimeRange(getFloat("InitialLifetime", "min", m->GetMinLifetime()), getFloat("InitialLifetime", "max", m->GetMaxLifetime()));
		else if (auto* m = dynamic_cast<InitialColorModule*>(module.get())) { m->SetMinColor(getV4("InitialColor", "min", m->GetMinColor())); m->SetMaxColor(getV4("InitialColor", "max", m->GetMaxColor())); }
		else if (auto* m = dynamic_cast<InitialScaleModule*>(module.get())) m->SetScaleRange(getV3("InitialScale", "min", m->GetMinScale()), getV3("InitialScale", "max", m->GetMaxScale()));
		else if (auto* m = dynamic_cast<GravityModule*>(module.get())) m->SetGravityRange(getV3("Gravity", "min", m->GetMinGravity()), getV3("Gravity", "max", m->GetMaxGravity()));
		else if (auto* m = dynamic_cast<DragModule*>(module.get())) m->SetDragRange(getFloat("Drag", "min", m->GetMinDrag()), getFloat("Drag", "max", m->GetMaxDrag()));
		else if (auto* m = dynamic_cast<ColorFadeModule*>(module.get())) m->SetColors(getV4("ColorFade", "start", m->GetStartColor()), getV4("ColorFade", "end", m->GetEndColor()));
		else if (auto* m = dynamic_cast<ScaleOverLifetimeModule*>(module.get())) m->SetScales(getV3("ScaleOverLifetime", "start", m->GetStartScale()), getV3("ScaleOverLifetime", "end", m->GetEndScale()));
		else if (auto* m = dynamic_cast<RotationOverLifetimeModule*>(module.get())) m->SetRotationSpeedRange(getFloat("RotationOverLifetime", "startSpeed", m->GetStartSpeed()), getFloat("RotationOverLifetime", "endSpeed", m->GetEndSpeed()));
		else if (auto* m = dynamic_cast<NoiseModule*>(module.get())) { m->SetStrength(getFloat("Noise", "strength", m->GetStrength())); m->SetFrequency(getFloat("Noise", "frequency", m->GetFrequency())); }
		else if (auto* m = dynamic_cast<VelocityOverLifetimeModule*>(module.get())) { m->SetStartMultiplier(getFloat("VelocityOverLifetime", "startMultiplier", m->GetStartMultiplier())); m->SetEndMultiplier(getFloat("VelocityOverLifetime", "endMultiplier", m->GetEndMultiplier())); }
		else if (auto* m = dynamic_cast<StretchByVelocityModule*>(module.get())) { m->SetStretchFactor(getFloat("StretchByVelocity", "stretchFactor", m->GetStretchFactor())); m->SetMinStretch(getFloat("StretchByVelocity", "minStretch", m->GetMinStretch())); m->SetMaxStretch(getFloat("StretchByVelocity", "maxStretch", m->GetMaxStretch())); }
		else if (auto* m = dynamic_cast<FlickerModule*>(module.get())) { m->SetFrequency(getFloat("Flicker", "frequency", m->GetFrequency())); m->SetMinAlpha(getFloat("Flicker", "minAlpha", m->GetMinAlpha())); m->SetMaxAlpha(getFloat("Flicker", "maxAlpha", m->GetMaxAlpha())); }
		else if (auto* m = dynamic_cast<AlphaFadeModule*>(module.get())) { m->SetStartAlpha(getFloat("AlphaFade", "startAlpha", m->GetStartAlpha())); m->SetEndAlpha(getFloat("AlphaFade", "endAlpha", m->GetEndAlpha())); }
	}
}

bool ParticleEmitter::SetDynamicBinding(DynamicParameterBinding binding)
{
	const ModuleDescriptor* descriptor = ModuleDescriptorRegistry::GetInstance().Find(binding.moduleId);
	if (!descriptor || binding.parameterId.empty() || binding.emitterParameter.size() > 128 || binding.keys.size() > 1024) return false;
	const auto schema = std::find_if(descriptor->parameters.begin(), descriptor->parameters.end(),
		[&](const ModuleParameterSchema& parameter) { return parameter.id == binding.parameterId; });
	if (schema == descriptor->parameters.end() || !schema->dynamicInput || schema->type != binding.type) return false;
	if (binding.mode == DynamicBindingMode::EmitterParameter && binding.emitterParameter.empty()) return false;
	std::sort(binding.keys.begin(), binding.keys.end(), [](const DynamicBindingKey& a, const DynamicBindingKey& b) { return a.time < b.time; });
	for (size_t index = 1; index < binding.keys.size();)
	{
		if (binding.keys[index - 1].time == binding.keys[index].time)
		{
			binding.keys[index - 1] = binding.keys[index];
			binding.keys.erase(binding.keys.begin() + static_cast<std::ptrdiff_t>(index));
		}
		else ++index;
	}
	auto existing = std::find_if(dynamicBindings_.begin(), dynamicBindings_.end(), [&](const DynamicParameterBinding& value)
	{
		return value.moduleId == binding.moduleId && value.parameterId == binding.parameterId;
	});
	if (existing == dynamicBindings_.end()) dynamicBindings_.push_back(std::move(binding));
	else *existing = std::move(binding);
	compiledEmitterDirty_ = true;
	return true;
}

bool ParticleEmitter::RemoveDynamicBinding(const std::string& moduleId, const std::string& parameterId)
{
	const size_t oldSize = dynamicBindings_.size();
	std::erase_if(dynamicBindings_, [&](const DynamicParameterBinding& value) { return value.moduleId == moduleId && value.parameterId == parameterId; });
	if (dynamicBindings_.size() != oldSize) compiledEmitterDirty_ = true;
	return dynamicBindings_.size() != oldSize;
}

const DynamicParameterBinding* ParticleEmitter::FindDynamicBinding(const std::string& moduleId, const std::string& parameterId) const
{
	auto binding = std::find_if(dynamicBindings_.begin(), dynamicBindings_.end(), [&](const DynamicParameterBinding& value)
	{
		return value.moduleId == moduleId && value.parameterId == parameterId;
	});
	return binding == dynamicBindings_.end() ? nullptr : &*binding;
}

void ParticleEmitter::SetRenderer(std::unique_ptr<IRenderer> renderer)
{
	if (renderer_)
	{
		// 古いレンダラーは即座に破棄せず、ゴミ箱（遅延破棄リスト）へ送る
		ParticleManager::GetInstance()->AddRendererToTrashBin(std::move(renderer_));
	}
	renderer_ = std::move(renderer);
}

void ParticleEmitter::SetSimulationMode(SimulationMode mode)
{
	if (simulationMode_ == mode) return;

	// シミュレーションモード切り替え時は、古いパーティクルデータをクリアする
	ClearParticles();
	if (gpuSimulator_) gpuSimulator_->ClearParticles();

	simulationMode_ = mode;

	if (mode == SimulationMode::GPU && !gpuSimulator_)
	{
		auto* pm = ParticleManager::GetInstance();
		gpuSimulator_ = std::make_unique<GPUSimulator>();
		gpuSimulator_->Initialize(pm->GetDxCommon(), pm->GetSrvManager(), maxParticles_);
		if (!gpuSimulator_->IsInitialized())
		{
			gpuSimulator_.reset();
			simulationMode_ = SimulationMode::CPU;
		}
	}
}

void ParticleEmitter::SpawnParticle(const Particle& particle)
{
	if (particles_.size() < maxParticles_)
	{
		Particle p = particle;
		p.id = nextParticleId_++;
		p.SetAlive(true);
		particles_.push_back(p);
	}
}

void ParticleEmitter::Play()
{
	Reset();
	enabled_ = true;
	isPaused_ = false;
	isEmitting_ = true;
}

void ParticleEmitter::Stop()
{
	isEmitting_ = false;

	if (inactiveResponse_ == InactiveResponse::Kill)
	{
		particles_.clear();
		if (gpuSimulator_) gpuSimulator_->ClearParticles();
	}
}

void ParticleEmitter::Pause()
{
	isPaused_ = true;
}

void ParticleEmitter::Resume()
{
	isPaused_ = false;
}

void ParticleEmitter::Reset()
{
	particles_.clear();
	if (gpuSimulator_) gpuSimulator_->ClearParticles();
	pureGpuLoopResetRequested_ = true;
	emitterAge_ = 0.0f;
	currentLoopCount_ = 0;
	isEmitting_ = true;
	delayElapsed_ = false;
	isPaused_ = false;
	hasPreviousPosition_ = false;
	nextParticleId_ = 0;

	if (renderer_)
	{
		renderer_->Reset();
	}

	// 全モジュールをリセット
	for (auto& module : modules_)
	{
		module->Reset();
	}
}

void ParticleEmitter::Restart()
{
	// particles_は残したまま、ライフサイクル状態だけ戻して再生成を開始
	emitterAge_ = 0.0f;
	currentLoopCount_ = 0;
	isEmitting_ = true;
	delayElapsed_ = false;
	isPaused_ = false;
	pureGpuLoopResetRequested_ = true;

	// モジュールの生成カウント等もリセット
	for (auto& module : modules_)
	{
		module->Reset();
	}
}

bool ParticleEmitter::IsComplete() const
{
	const bool gpuMayBeAlive = simulationMode_ == SimulationMode::GPU && gpuSimulator_ && gpuSimulator_->MayHaveLiveParticles();
	return !isEmitting_ && particles_.empty() && !gpuMayBeAlive;
}

uint32_t ParticleEmitter::GetActiveParticleCount() const
{
	return simulationMode_ == SimulationMode::GPU && gpuSimulator_ && gpuSimulator_->IsPureGPUPath()
		? (gpuSimulator_->MayHaveLiveParticles() ? gpuSimulator_->GetMaxParticles() : 0u)
		: static_cast<uint32_t>(particles_.size());
}

void ParticleEmitter::BindGPUEventSource(ParticleEmitter* source)
{
	if (!gpuSimulator_) return;
	if (source && source->gpuSimulator_)
	{
		gpuSimulator_->SetEventSource(source->gpuSimulator_.get(), gpuEventTrigger_, gpuEventProbability_,
			gpuEventInheritVelocity_, gpuEventVelocityScale_, gpuEventInheritColor_);
	}
	else
	{
		gpuSimulator_->ClearEventSource();
	}
}

void ParticleEmitter::RemoveModule(size_t index)
{
	if (index < modules_.size())
	{
		modules_.erase(modules_.begin() + static_cast<ptrdiff_t>(index));
		modulesSorted_ = false;
		compiledEmitterDirty_ = true;
	}
}

void ParticleEmitter::MoveModuleUp(size_t index)
{
	if (index > 0 && index < modules_.size())
	{
		std::swap(modules_[index], modules_[index - 1]);
		modulesSorted_ = false;
		compiledEmitterDirty_ = true;
	}
}

void ParticleEmitter::MoveModuleDown(size_t index)
{
	if (index < modules_.size() - 1)
	{
		std::swap(modules_[index], modules_[index + 1]);
		modulesSorted_ = false;
		compiledEmitterDirty_ = true;
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
	if (compiledEmitterDirty_)
	{
		compiledEmitter_ = ModuleDescriptorRegistry::GetInstance().Compile(modules_, dynamicBindings_);
		compiledEmitterDirty_ = false;
	}
}

void ParticleEmitter::UpdateCPU(float deltaTime)
{
	ParticleScopeTimer timer(ParticleProfileScope::EmitterUpdateCpuPerCall);

	// 一時停止中は何もしない
	if (isPaused_) return;

	// モジュールをソート（必要な場合のみ）
	SortModulesByPriority();

	//===== ライフサイクル管理 =====//

	// 遅延チェック
	if (!delayElapsed_)
	{
		emitterAge_ += deltaTime;
		if (emitterAge_ < startDelay_)
		{
			// 遅延中はパーティクルの更新のみ（生成しない）
			goto update_particles;
		}
		delayElapsed_ = true;
		emitterAge_ = 0.0f; // Duration計測開始
	}

	// Duration チェック（duration_ > 0 のときのみ）
	if (isEmitting_ && duration_ > 0.0f)
	{
		emitterAge_ += deltaTime;

		if (emitterAge_ >= duration_)
		{
			// ループ処理
			if (loopBehavior_ == LoopBehavior::Infinite)
			{
				emitterAge_ = 0.0f;  // ループ継続
				for (auto& module : modules_)
				{
					module->Reset();
				}
			}
			else if (loopBehavior_ == LoopBehavior::Multiple)
			{
				currentLoopCount_++;
				if (currentLoopCount_ < loopCount_)
				{
					emitterAge_ = 0.0f;  // 次のループ
					for (auto& module : modules_)
					{
						module->Reset();
					}
				}
				else
				{
					isEmitting_ = false;  // 生成終了
				}
			}
			else // Once
			{
				isEmitting_ = false;  // 生成終了
			}
		}
	}
	// duration_ == 0 の場合は無限なのでemitterAge_を更新しない

	{
		ParticleContext context;
		context.particles = &particles_;
		context.deltaTime = deltaTime;
		context.emitterPosition = position_;
		context.followTarget = followTarget_;
		context.spawnCount = 0;
		context.maxParticles = maxParticles_;

		// 移動検出
		bool isMoving = true;
		if (spawnOnlyWhenMoving_)
		{
			if (hasPreviousPosition_)
			{
				float dx = position_.x - previousPosition_.x;
				float dy = position_.y - previousPosition_.y;
				float dz = position_.z - previousPosition_.z;
				float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
				isMoving = distance >= minMoveDistance_;
			}
			previousPosition_ = position_;
			hasPreviousPosition_ = true;
		}

		// Spawnフェーズのモジュールを実行（有効 && 生成中 && 移動時）
		if (enabled_ && isEmitting_ && isMoving)
		{
			ExecuteSpawnModules(context);
		}

		// Updateフェーズのモジュールを実行
		ExecuteUpdateModules(context);
	}

	// Once + duration=0: Spawnモジュールが全完了したら生成を止める
	// （durationチェックブロックは duration>0 のときのみ実行されるため、
	//   duration=0 のままOnceを設定した場合ここで停止させる）
	if (isEmitting_ && duration_ == 0.0f && loopBehavior_ == LoopBehavior::Once)
	{
		bool hasSpawnModule = false;
		bool allSpawnDone  = true;
		for (const auto& module : modules_)
		{
			if (module->GetPhase() == ModulePhase::Spawn)
			{
				hasSpawnModule = true;
				if (!module->IsComplete())
				{
					allSpawnDone = false;
					break;
				}
			}
		}
		if (hasSpawnModule && allSpawnDone)
		{
			isEmitting_ = false;
		}
	}

update_particles:
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
	ParticleScopeTimer timer(ParticleProfileScope::EmitterUpdateGpuPerCall);

	if (gpuSimulator_ && gpuSimulator_->IsInitialized())
	{
		const bool pureGpu = renderer_ &&
			(renderer_->GetType() == RendererType::Sprite || renderer_->GetType() == RendererType::Mesh || renderer_->GetType() == RendererType::Ribbon) &&
			gpuSimulator_->SupportsPureGPU(modules_, renderer_->GetType());
		if (pureGpu)
		{
			UpdatePureGPULifecycle(deltaTime);

			Vector3 gravity = { 0.0f, 0.0f, 0.0f };
			for (const auto& module : modules_)
			{
				if (auto* gravityModule = dynamic_cast<GravityModule*>(module.get()))
				{
					gravity = gravityModule->GetMinGravity();
					break;
				}
			}
			gpuSimulator_->SetGravity(gravity);
			gpuSimulator_->SetIsBillboard(renderer_->GetBillboard());
			gpuSimulator_->SetEmitterPosition(position_);
			if (auto* trail = dynamic_cast<TrailRenderer*>(renderer_.get()))
			{
				uint32_t ribbonGroupCount = 1;
				for (const auto& module : modules_)
				{
					if (const auto* assign = dynamic_cast<const AssignRibbonIdModule*>(module.get()))
					{
						ribbonGroupCount = assign->GetGroupCount();
						break;
					}
				}
				gpuSimulator_->SetRibbonParameters(true, trail->GetTrailWidth(), trail->GetWidthFade(), trail->GetAlphaFade(), ribbonGroupCount);
			}
			else
			{
				gpuSimulator_->SetRibbonParameters(false, 0.5f, false, false);
			}

			Matrix4x4 emitterWorld = followTarget_
				? MakeAffineMatrix(followTarget_->scale, followTarget_->rotate, position_)
				: MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, position_);
			gpuSimulator_->DispatchPure(deltaTime, camera, modules_, emitterWorld,
				static_cast<uint32_t>(simulationSpace_), enabled_ && isEmitting_ && delayElapsed_ && !isPaused_,
				pureGpuLoopResetRequested_);
			pureGpuLoopResetRequested_ = false;
			particles_.clear();
			return;
		}

		// Unsupported combinations use the CPU implementation directly. The old
		// hybrid path copied the full particle pool GPU -> CPU -> GPU every frame.
		gpuSimulator_->ClearParticles();
		UpdateCPU(deltaTime);
	}
	else
	{
		UpdateCPU(deltaTime);
	}
}

void ParticleEmitter::UpdatePureGPULifecycle(float deltaTime)
{
	if (isPaused_) return;
	SortModulesByPriority();

	if (!delayElapsed_)
	{
		emitterAge_ += deltaTime;
		if (emitterAge_ < startDelay_) return;
		delayElapsed_ = true;
		emitterAge_ = 0.0f;
	}

	if (!isEmitting_ || duration_ <= 0.0f) return;
	emitterAge_ += deltaTime;
	if (emitterAge_ < duration_) return;

	if (loopBehavior_ == LoopBehavior::Infinite)
	{
		emitterAge_ = 0.0f;
		pureGpuLoopResetRequested_ = true;
		return;
	}
	if (loopBehavior_ == LoopBehavior::Multiple && ++currentLoopCount_ < loopCount_)
	{
		emitterAge_ = 0.0f;
		pureGpuLoopResetRequested_ = true;
		return;
	}
	isEmitting_ = false;
}

void ParticleEmitter::UpdateGPUSpawns(float deltaTime)
{
	ParticleScopeTimer timer(ParticleProfileScope::UpdateGPUSpawns);

	if (isPaused_) return;

	SortModulesByPriority();

	//===== ライフサイクル管理 =====//
	if (!delayElapsed_)
	{
		emitterAge_ += deltaTime;
		if (emitterAge_ < startDelay_)
		{
			return;
		}
		delayElapsed_ = true;
		emitterAge_ = 0.0f;
	}

	if (isEmitting_ && duration_ > 0.0f)
	{
		emitterAge_ += deltaTime;
		if (emitterAge_ >= duration_)
		{
			if (loopBehavior_ == LoopBehavior::Infinite)
			{
				emitterAge_ = 0.0f;
				for (auto& module : modules_)
				{
					module->Reset();
				}
			}
			else if (loopBehavior_ == LoopBehavior::Multiple)
			{
				currentLoopCount_++;
				if (currentLoopCount_ < loopCount_)
				{
					emitterAge_ = 0.0f;
					for (auto& module : modules_)
					{
						module->Reset();
					}
				}
				else
				{
					isEmitting_ = false;
				}
			}
			else // Once
			{
				isEmitting_ = false;
			}
		}
	}

	{
		ParticleContext context;
		context.particles = &particles_;
		context.deltaTime = deltaTime;
		context.emitterPosition = position_;
		context.followTarget = followTarget_;
		context.spawnCount = 0;
		context.maxParticles = maxParticles_;

		// 移動検出
		bool isMoving = true;
		if (spawnOnlyWhenMoving_)
		{
			if (hasPreviousPosition_)
			{
				float dx = position_.x - previousPosition_.x;
				float dy = position_.y - previousPosition_.y;
				float dz = position_.z - previousPosition_.z;
				float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
				isMoving = distance >= minMoveDistance_;
			}
			previousPosition_ = position_;
			hasPreviousPosition_ = true;
		}

		// Spawnフェーズのモジュールを実行して新規パーティクルを追加
		if (enabled_ && isEmitting_ && isMoving)
		{
			ExecuteSpawnModules(context);
		}
	}

	if (isEmitting_ && duration_ == 0.0f && loopBehavior_ == LoopBehavior::Once)
	{
		bool hasSpawnModule = false;
		bool allSpawnDone  = true;
		for (const auto& module : modules_)
		{
			if (module->GetPhase() == ModulePhase::Spawn)
			{
				hasSpawnModule = true;
				if (!module->IsComplete())
				{
					allSpawnDone = false;
					break;
				}
			}
		}
		if (hasSpawnModule && allSpawnDone)
		{
			isEmitting_ = false;
		}
	}
}

void ParticleEmitter::RemoveDeadParticles()
{
	ParticleScopeTimer timer(ParticleProfileScope::RemoveDeadParticles);

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
	context.parameters = &parameterStore_;
	compiledEmitter_.Execute(ModulePhase::Spawn, context);
}

void ParticleEmitter::ExecuteUpdateModules(ParticleContext& context)
{
	context.parameters = &parameterStore_;
	compiledEmitter_.Execute(ModulePhase::Update, context);
}

IModule* ParticleEmitter::GetModuleByName(const std::string& name)
{
	for (auto& module : modules_)
	{
		if (module->GetName() == name)
		{
			return module.get();
		}
	}
	return nullptr;
}

const IModule* ParticleEmitter::GetModuleByName(const std::string& name) const
{
	for (const auto& module : modules_)
	{
		if (module->GetName() == name)
		{
			return module.get();
		}
	}
	return nullptr;
}
} // namespace KCE
