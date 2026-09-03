#include "ParticleManager.h"
#include "ParticleEffect.h"
#include "base/DirectXCommon.h"
#include "manager/system/SrvManager.h"
#include "manager/scene/CameraManager.h"
#include "manager/effect/ParticlePipelineManager.h"
#include "effects/particle/renderer/IRenderer.h"
#include "effects/particle/gpu/GPUSimulator.h"
#include "time/TimeManager.h"
#include "time/Timer.h"
#include <algorithm>
#include <chrono>
#include "effects/particle/diagnostics/ParticleDiagnostics.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "manager/editor/DebugUIManager.h"

#endif

namespace KCE
{

ParticleManager::~ParticleManager() = default;

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

#ifdef USE_IMGUI
	DebugUIManager::GetInstance()->RegisterDebugUI(this, "Particle Manager", [this]() { this->DrawImGui(); }, DebugUIArea::Inspector);
#endif
}

void ParticleManager::Finalize()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->UnregisterDebugUI(this);
	}
#endif
	effects_.clear();
	effectPools_.clear();
	rendererTrashBin_.clear();
	simulatorTrashBin_.clear();
	effectTrashBin_.clear();
	emitters_.clear();
	effectDefinitions_.clear();
	pipelineManager_.reset();
}

void ParticleManager::Update(CameraManager* camera)
{
	auto* diag = ParticleDiagnostics::GetInstance();
	diag->EndFrameCounters();
	diag->BeginFrameCounters();

	ParticleScopeTimer timer(ParticleProfileScope::ManagerUpdate);

	// Fence completion, rather than frame count, owns GPU resource lifetime.
	// This remains correct when PostDraw stops waiting every frame or when
	// multiple frames are in flight.
	const uint64_t completedFence = dxCommon_ ? dxCommon_->GetCompletedFenceValue() : UINT64_MAX;
	std::erase_if(rendererTrashBin_, [completedFence](const RetiredRenderer& retired) { return retired.fenceValue <= completedFence; });
	std::erase_if(simulatorTrashBin_, [completedFence](const RetiredSimulator& retired) { return retired.fenceValue <= completedFence; });
	std::erase_if(effectTrashBin_, [completedFence](const RetiredEffect& retired) { return retired.fenceValue <= completedFence; });

	float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;
	float unscaledDeltaTime = TimeManager::GetInstance().GetGameContext().realDeltaTime;

	// エフェクトの更新（再生中 or 残存パーティクルがある間は継続）
	for (auto& effect : effects_)
	{
		if (effect->IsPlaying() || !effect->IsFinished())
		{
			float dt = (effect->GetDeltaTimeType() == DeltaTimeType::RealDeltaTime) ? unscaledDeltaTime : deltaTime;
			effect->Update(dt, camera);
		}
	}

	// 直接追加されたエミッターの更新（後方互換）
	for (auto& emitter : emitters_)
	{
		emitter->Update(deltaTime, camera);
	}

	// 終了したエフェクトを削除
	RemoveFinishedEffects();
}

void ParticleManager::Draw()
{
	if (effects_.empty() && emitters_.empty()) return;

	dxCommon_->GetCommandList()->SetGraphicsRootSignature(pipelineManager_->GetRootSignature());
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	// エフェクトの描画
	for (auto& effect : effects_)
	{
		effect->Draw(dxCommon_, srvManager_);
	}

	// 直接追加されたエミッターの描画（後方互換）
	for (auto& emitter : emitters_)
	{
		emitter->Draw(dxCommon_, srvManager_);
	}
}

void ParticleManager::DrawImGui()
{
#ifdef USE_IMGUI

	// 統計情報
	uint32_t totalParticles = 0;
	uint32_t totalEmitters = 0;

	// エフェクト内のエミッターをカウント
	for (const auto& effect : effects_)
	{
		for (size_t i = 0; i < effect->GetEmitterCount(); ++i)
		{
			auto* emitter = effect->GetEmitter(i);
			if (emitter)
			{
				totalParticles += emitter->GetActiveParticleCount();
				totalEmitters++;
			}
		}
	}

	// 直接追加されたエミッター
	for (const auto& emitter : emitters_)
	{
		totalParticles += emitter->GetActiveParticleCount();
		totalEmitters++;
	}

	ImGui::Text("Total Particles: %u", totalParticles);
	ImGui::Text("Total Emitters: %u", totalEmitters);
	ImGui::Text("Active Effects: %d", static_cast<int>(effects_.size()));

	// SRV使用状況（Active = 実使用中、HWM = 確保した最大インデックス）
	uint32_t srvActive = srvManager_->GetActiveSRVCount();
	uint32_t srvHwm    = srvManager_->GetUseIndex();
	uint32_t srvMax    = SrvManager::kMaxSRVCount;
	ImGui::Text("SRV Active: %u / %u  (HWM: %u)", srvActive, srvMax, srvHwm);
	ImGui::Separator();



	// エフェクトごとの詳細
	if (ImGui::CollapsingHeader("Effects", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (size_t effectIdx = 0; effectIdx < effects_.size(); ++effectIdx)
		{
			auto& effect = effects_[effectIdx];
			ImGui::PushID(static_cast<int>(effectIdx));

			bool isPlaying = effect->IsPlaying();
			if (ImGui::TreeNode(effect->GetName().c_str()))
			{
				if (ImGui::Checkbox("Playing", &isPlaying))
				{
					if (isPlaying) effect->Play();
					else effect->Stop();
				}

				ImGui::SameLine();
				if (ImGui::Button("Reset"))
				{
					effect->Reset();
					effect->Play();
				}

				// エミッターごとの詳細
				for (size_t i = 0; i < effect->GetEmitterCount(); ++i)
				{
					auto* emitter = effect->GetEmitter(i);
					if (emitter)
					{
						ImGui::Text("  [%s] Particles: %d", emitter->GetName().c_str(),
							static_cast<int>(emitter->GetActiveParticleCount()));
					}
				}

				ImGui::TreePop();
			}

			ImGui::PopID();
		}
	}

	// 直接追加されたエミッター
	if (!emitters_.empty() && ImGui::CollapsingHeader("Standalone Emitters", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (size_t i = 0; i < emitters_.size(); ++i)
		{
			auto& emitter = emitters_[i];
			ImGui::PushID(static_cast<int>(1000 + i));

			bool isEnabled = emitter->IsEnabled();
			if (ImGui::TreeNode(emitter->GetName().c_str()))
			{
				if (ImGui::Checkbox("Playing", &isEnabled))
				{
					emitter->SetEnabled(isEnabled);
				}

				ImGui::SameLine();
				if (ImGui::Button("Clear"))
				{
					emitter->ClearParticles();
				}

				ImGui::Text("Particles: %d", static_cast<int>(emitter->GetActiveParticleCount()));
				ImGui::Text("Mode: %s", emitter->GetSimulationMode() == SimulationMode::GPU ? "GPU" : "CPU");

				ImGui::TreePop();
			}

			ImGui::PopID();
		}
	}

#endif
}

//===== エフェクトのロード（推奨API）=====//

ParticleEffect* ParticleManager::Load(const std::string& name, const std::string& jsonPath)
{
	// 既に同名のエフェクトがあれば返す
	if (auto* existing = GetEffect(name))
	{
		return existing;
	}

	// JSONからロード
	auto effect = ParticleEffect::LoadFromFile(jsonPath);
	if (!effect)
	{
		return nullptr;
	}

	// Playで複数再生する時のために定義として保存しておく
	effectDefinitions_[name] = jsonPath;

	effect->Initialize(name);
	// Play()は呼ばない（非アクティブ状態で保持）
	// 手動管理する場合は自動削除オフにするのが安全
	effect->SetAutoRemove(false); 

	ParticleEffect* ptr = effect.get();
	effects_.push_back(std::move(effect));
	return ptr;
}

ParticleEffect* ParticleManager::CreateEmpty(const std::string& name)
{
	// 既に同名のエフェクトがあれば返す
	if (auto* existing = GetEffect(name))
	{
		return existing;
	}

	auto effect = std::make_unique<ParticleEffect>();
	effect->Initialize(name);
	// Play()は呼ばない（非アクティブ状態で保持）
	// 手動管理する場合は自動削除オフにするのが安全
	effect->SetAutoRemove(false);

	ParticleEffect* ptr = effect.get();
	effects_.push_back(std::move(effect));
	return ptr;
}

bool ParticleManager::HasEffect(const std::string& name) const
{
	for (const auto& effect : effects_)
	{
		if (effect->GetName() == name)
		{
			return true;
		}
	}
	return false;
}

//===== エフェクト定義の管理（後方互換）=====//

void ParticleManager::LoadEffectDefinition(const std::string& name, const std::string& jsonPath)
{
	effectDefinitions_[name] = jsonPath;
}

ParticleEffect* ParticleManager::Play(const std::string& effectName, const Vector3& position)
{
	// プールから取得を試みる
	auto poolIt = effectPools_.find(effectName);
	if (poolIt != effectPools_.end() && !poolIt->second.empty())
	{
		auto effect = std::move(poolIt->second.back());
		poolIt->second.pop_back();

		effect->ResetForPool();
		effect->SetPosition(position);
		effect->SetAutoRemove(true);
		effect->Play();

		ParticleEffect* ptr = effect.get();
		effects_.push_back(std::move(effect));
		return ptr;
	}

	// 定義があればロードして新規生成
	auto it = effectDefinitions_.find(effectName);
	if (it != effectDefinitions_.end())
	{
		auto effect = ParticleEffect::LoadFromFile(it->second);
		if (!effect) return nullptr;

		effect->Initialize(effectName);
		effect->SetPosition(position);
		effect->SetAutoRemove(true);
		effect->Play();

		ParticleEffect* ptr = effect.get();
		effects_.push_back(std::move(effect));
		return ptr;
	}

	return nullptr;
}

ParticleEffect* ParticleManager::Play(const std::string& effectName, Transform* followTarget)
{
	// プールから取得を試みる
	auto poolIt = effectPools_.find(effectName);
	if (poolIt != effectPools_.end() && !poolIt->second.empty())
	{
		auto effect = std::move(poolIt->second.back());
		poolIt->second.pop_back();

		effect->ResetForPool();
		effect->SetFollowTarget(followTarget);
		effect->SetAutoRemove(true);
		effect->Play();

		ParticleEffect* ptr = effect.get();
		effects_.push_back(std::move(effect));
		return ptr;
	}

	// 定義があればロードして新規生成
	auto it = effectDefinitions_.find(effectName);
	if (it != effectDefinitions_.end())
	{
		auto effect = ParticleEffect::LoadFromFile(it->second);
		if (!effect) return nullptr;

		effect->Initialize(effectName);
		effect->SetFollowTarget(followTarget);
		effect->SetAutoRemove(true);
		effect->Play();

		ParticleEffect* ptr = effect.get();
		effects_.push_back(std::move(effect));
		return ptr;
	}

	return nullptr;
}

void ParticleManager::Warmup(const std::string& effectName, size_t count)
{
	auto it = effectDefinitions_.find(effectName);
	if (it == effectDefinitions_.end()) return;

	auto& pool = effectPools_[effectName];
	for (size_t i = 0; i < count; ++i)
	{
		auto effect = ParticleEffect::LoadFromFile(it->second);
		if (effect)
		{
			effect->Initialize(effectName);
			effect->ResetForPool();
			pool.push_back(std::move(effect));
		}
	}
}


void ParticleManager::AddEffect(std::unique_ptr<ParticleEffect> effect)
{
	effects_.push_back(std::move(effect));
}

ParticleEffect* ParticleManager::GetEffect(const std::string& name)
{
	for (auto& effect : effects_)
	{
		if (effect->GetName() == name)
		{
			return effect.get();
		}
	}
	return nullptr;
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

void ParticleManager::StopAll()
{
	for (auto& effect : effects_)
	{
		effect->Stop();
	}
}

void ParticleManager::Clear()
{
	effects_.clear();
	effectPools_.clear();
	emitters_.clear();
}

size_t ParticleManager::GetPooledEffectCount() const
{
	size_t count = 0;
	for (const auto& [name, pool] : effectPools_)
	{
		(void)name;
		count += pool.size();
	}
	return count;
}

void ParticleManager::PurgeEffectPools()
{
	const uint64_t fence = dxCommon_ ? dxCommon_->GetNextFenceValue() : 0;
	for (auto& [name, pool] : effectPools_)
	{
		(void)name;
		for (auto& effect : pool)
		{
			if (effect) effectTrashBin_.push_back({ fence, std::move(effect) });
		}
	}
	effectPools_.clear();
}

void ParticleManager::RemoveEffect(ParticleEffect* effect)
{
	for (auto it = effects_.begin(); it != effects_.end();)
	{
		if (it->get() == effect)
		{
			std::string name = (*it)->GetName();
			(*it)->ResetForPool();
			auto& pool = effectPools_[name];
			// Avoid unbounded descriptor retention when many one-shot asset names
			// are created. Excess instances are retired behind the GPU fence.
			if (pool.size() < 2 && GetPooledEffectCount() < 16)
				pool.push_back(std::move(*it));
			else
				effectTrashBin_.push_back({ dxCommon_ ? dxCommon_->GetNextFenceValue() : 0, std::move(*it) });
			effects_.erase(it);
			break;
		}
		else
		{
			++it;
		}
	}
}

void ParticleManager::RemoveFinishedEffects()
{
	for (auto it = effects_.begin(); it != effects_.end();)
	{
		if ((*it)->IsFinished() && (*it)->IsAutoRemove())
		{
			std::string name = (*it)->GetName();
			(*it)->ResetForPool();
			auto& pool = effectPools_[name];
			if (pool.size() < 2 && GetPooledEffectCount() < 16)
				pool.push_back(std::move(*it));
			else
				effectTrashBin_.push_back({ dxCommon_ ? dxCommon_->GetNextFenceValue() : 0, std::move(*it) });
			it = effects_.erase(it);
		}
		else
		{
			++it;
		}
	}
}

ParticleEffect* ParticleManager::GetEffect(size_t index)
{
	return index < effects_.size() ? effects_[index].get() : nullptr;
}

const ParticleEffect* ParticleManager::GetEffect(size_t index) const
{
	return index < effects_.size() ? effects_[index].get() : nullptr;
}

bool ParticleManager::RemoveEffect(const std::string& name)
{
	bool removed = false;
	for (auto it = effects_.begin(); it != effects_.end();)
	{
		if ((*it)->GetName() == name)
		{
			std::string effectName = (*it)->GetName();
			(*it)->ResetForPool();
			auto& pool = effectPools_[effectName];
			if (pool.size() < 2 && GetPooledEffectCount() < 16)
				pool.push_back(std::move(*it));
			else
				effectTrashBin_.push_back({ dxCommon_ ? dxCommon_->GetNextFenceValue() : 0, std::move(*it) });
			it = effects_.erase(it);
			removed = true;
		}
		else
		{
			++it;
		}
	}
	return removed;
}

void ParticleManager::AddRendererToTrashBin(std::unique_ptr<IRenderer> renderer)
{
	if (renderer)
	{
		rendererTrashBin_.push_back({ dxCommon_ ? dxCommon_->GetNextFenceValue() : 0, std::move(renderer) });
	}
}

void ParticleManager::AddSimulatorToTrashBin(std::unique_ptr<GPUSimulator> simulator)
{
	if (simulator)
	{
		simulatorTrashBin_.push_back({ dxCommon_ ? dxCommon_->GetNextFenceValue() : 0, std::move(simulator) });
	}
}
} // namespace KCE
