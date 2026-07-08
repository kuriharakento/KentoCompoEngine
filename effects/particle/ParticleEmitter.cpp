#include "ParticleEmitter.h"
#include "effects/particle/module/IModule.h"
#include "effects/particle/module/spawn/SubEmitterModule.h"
#include "effects/particle/module/update/UpdateModules.h"
#include "effects/particle/renderer/IRenderer.h"
#include "effects/particle/gpu/GPUSimulator.h"
#include "effects/particle/ParticleManager.h"
#include "base/DirectXCommon.h"
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
	if (!isPaused_)
	{
		activeTime_ += deltaTime;
	}

	if (wasEmitting_ && !isEmitting_)
	{
		stopTime_ = activeTime_;
	}
	wasEmitting_ = isEmitting_;

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
		bool skip = !isEmitting_ && (activeTime_ > stopTime_ + maxLifetime_);
		if (!skip)
		{
			UpdateGPU(deltaTime, camera);
		}
		else
		{
			if (gpuSimulator_)
			{
				gpuSimulator_->ClearParticles();
			}
		}
	}

	if (renderer_)
	{
		if (simulationMode_ == SimulationMode::GPU && gpuSimulator_)
		{
			uint32_t currentElementCount = renderer_->GetElementCount();
			if (lastElementCount_ != currentElementCount)
			{
				gpuSimulator_->InitializeIndirectArgs(renderer_->GetType(), currentElementCount);
				lastElementCount_ = currentElementCount;
			}
			renderer_->SetGPUMode(true, gpuSimulator_->GetRenderSrvIndex(), gpuSimulator_->GetParticleCount(), gpuSimulator_->GetIndirectArgsBuffer());
		}
		else
		{
			renderer_->SetGPUMode(false, 0, 0);
		}
		renderer_->Update(particles_, camera);
	}
}

void ParticleEmitter::Draw(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	if (simulationMode_ == SimulationMode::GPU)
	{
		bool skip = !isEmitting_ && (activeTime_ > stopTime_ + maxLifetime_);
		if (skip) return;
	}

	if (renderer_)
	{
		if (simulationMode_ == SimulationMode::GPU && gpuSimulator_)
		{
			uint32_t currentElementCount = renderer_->GetElementCount();
			if (lastElementCount_ != currentElementCount)
			{
				gpuSimulator_->InitializeIndirectArgs(renderer_->GetType(), currentElementCount);
				lastElementCount_ = currentElementCount;
			}
			renderer_->SetGPUMode(true, gpuSimulator_->GetRenderSrvIndex(), gpuSimulator_->GetParticleCount(), gpuSimulator_->GetIndirectArgsBuffer());
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

void ParticleEmitter::SetMaxParticles(uint32_t max)
{
	if (maxParticles_ == max) return;
	maxParticles_ = max;
	particles_.reserve(maxParticles_);

	// GPUシミュレーターが存在する場合は再初期化（再確保）する
	if (gpuSimulator_)
	{
		auto* pm = ParticleManager::GetInstance();
		gpuSimulator_ = std::make_unique<GPUSimulator>();
		gpuSimulator_->Initialize(pm->GetDxCommon(), pm->GetSrvManager(), maxParticles_);
	}
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
	emitterAge_ = 0.0f;
	currentLoopCount_ = 0;
	isEmitting_ = true;
	delayElapsed_ = false;
	isPaused_ = false;
	hasPreviousPosition_ = false;
	nextParticleId_ = 0;
	activeTime_ = 0.0f;
	stopTime_ = 0.0f;
	wasEmitting_ = true;
	lastElementCount_ = 0;

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
	activeTime_ = 0.0f;
	stopTime_ = 0.0f;
	wasEmitting_ = true;
	lastElementCount_ = 0;

	// モジュールの生成カウント等もリセット
	for (auto& module : modules_)
	{
		module->Reset();
	}
}

bool ParticleEmitter::IsComplete() const
{
	return !isEmitting_ && particles_.empty();
}

void ParticleEmitter::RemoveModule(size_t index)
{
	if (index < modules_.size())
	{
		modules_.erase(modules_.begin() + static_cast<ptrdiff_t>(index));
		modulesSorted_ = false;
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
	if (gpuSimulator_)
	{
		// 1. CPU側のパーティクルリストをクリア（新規スポンのみを入れるため）
		particles_.clear();

		// 2. CPU側でライフサイクル更新と新規スポン処理を行う
		// これにより、このフレームで新規発生したパーティクルのみが particles_ に格納される
		UpdateGPUSpawns(deltaTime);

		// 重力モジュールの値をGPUシミュレーターに転送
		Vector3 gravity = { 0.0f, 0.0f, 0.0f };
		for (const auto& module : modules_)
		{
			if (module->GetName() == std::string("Gravity"))
			{
				auto* gravityModule = dynamic_cast<GravityModule*>(module.get());
				if (gravityModule)
				{
					gravity = gravityModule->GetMinGravity();
				}
				break;
			}
		}
		gpuSimulator_->SetGravity(gravity);

		// レンダラーのビルボード設定をGPUシミュレーターに転送
		bool isBillboard = true;
		if (renderer_)
		{
			isBillboard = renderer_->GetBillboard();
		}
		gpuSimulator_->SetIsBillboard(isBillboard);

		// 3. 新規スポンデータがあればGPUにアップロード
		// 新規スポンが0件でもDispatchは実行（生存パーティクルのシミュレーションのため）
		gpuSimulator_->UploadParticles(particles_);
		
		// エミッターのワールド行列を計算
		Matrix4x4 emitterWorld;
		if (followTarget_)
		{
			emitterWorld = MakeAffineMatrix(followTarget_->scale, followTarget_->rotate, position_);
		}
		else
		{
			emitterWorld = MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, position_);
		}

		// 4. GPUシミュレーション実行 (常に実行、生存数等の更新をGPUで完結させるため)
		gpuSimulator_->SetEmitterPosition(position_);
		gpuSimulator_->Dispatch(deltaTime, camera, modules_, emitterWorld, static_cast<uint32_t>(simulationSpace_));

		// 5. アップロード完了したため、次のフレームに備えてCPU側新規スポンデータをクリア
		particles_.clear();
	}
	else
	{
		UpdateCPU(deltaTime);
	}
}

void ParticleEmitter::UpdateGPUSpawns(float deltaTime)
{
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
