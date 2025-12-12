#include "ParticleManager.h"
#include "ParticleEffect.h"
#include "base/DirectXCommon.h"
#include "manager/system/SrvManager.h"
#include "manager/scene/CameraManager.h"
#include "manager/effect/ParticlePipelineManager.h"
#include "time/TimeManager.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

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
	effects_.clear();
	emitters_.clear();
	effectDefinitions_.clear();
	pipelineManager_.reset();
}

void ParticleManager::Update(CameraManager* camera)
{
	float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;

	// エフェクトの更新
	for (auto& effect : effects_)
	{
		effect->Update(deltaTime, camera);
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
	// パイプラインステートは各レンダラーで設定する
	// dxCommon_->GetCommandList()->SetPipelineState(pipelineManager_->GetPipelineState(BlendMode::Additive));

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

	ImGui::Begin("Particle Manager");

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
				totalParticles += static_cast<uint32_t>(emitter->GetParticles().size());
				totalEmitters++;
			}
		}
	}

	// 直接追加されたエミッター
	for (const auto& emitter : emitters_)
	{
		totalParticles += static_cast<uint32_t>(emitter->GetParticles().size());
		totalEmitters++;
	}

	ImGui::Text("Total Particles: %u", totalParticles);
	ImGui::Text("Total Emitters: %u", totalEmitters);
	ImGui::Text("Active Effects: %d", static_cast<int>(effects_.size()));
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
							static_cast<int>(emitter->GetParticles().size()));
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

				ImGui::Text("Particles: %d", static_cast<int>(emitter->GetParticles().size()));
				ImGui::Text("Mode: %s", emitter->GetSimulationMode() == SimulationMode::GPU ? "GPU" : "CPU");

				ImGui::TreePop();
			}

			ImGui::PopID();
		}
	}

	ImGui::End();
#endif
}

void ParticleManager::LoadEffectDefinition(const std::string& name, const std::string& jsonPath)
{
	effectDefinitions_[name] = jsonPath;
}

ParticleEffect* ParticleManager::Play(const std::string& effectName, const Vector3& position)
{
	// 定義があればJSONから読み込み
	auto it = effectDefinitions_.find(effectName);
	if (it != effectDefinitions_.end())
	{
		auto effect = ParticleEffect::LoadFromFile(it->second);
		effect->Initialize(effectName);
		effect->SetPosition(position);
		effect->Play();
		
		ParticleEffect* ptr = effect.get();
		effects_.push_back(std::move(effect));
		return ptr;
	}

	// 定義がない場合は空のエフェクトを作成
	auto effect = std::make_unique<ParticleEffect>();
	effect->Initialize(effectName);
	effect->SetPosition(position);
	effect->Play();

	ParticleEffect* ptr = effect.get();
	effects_.push_back(std::move(effect));
	return ptr;
}

ParticleEffect* ParticleManager::Play(const std::string& effectName, Transform* followTarget)
{
	// 定義があればJSONから読み込み
	auto it = effectDefinitions_.find(effectName);
	std::unique_ptr<ParticleEffect> effect;
	
	if (it != effectDefinitions_.end())
	{
		effect = ParticleEffect::LoadFromFile(it->second);
	}
	else
	{
		effect = std::make_unique<ParticleEffect>();
	}
	
	effect->Initialize(effectName);
	effect->SetFollowTarget(followTarget);
	effect->Play();

	ParticleEffect* ptr = effect.get();
	effects_.push_back(std::move(effect));
	return ptr;
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
	emitters_.clear();
}

void ParticleManager::RemoveEffect(ParticleEffect* effect)
{
	effects_.erase(
		std::remove_if(effects_.begin(), effects_.end(),
			[effect](const std::unique_ptr<ParticleEffect>& e) {
				return e.get() == effect;
			}),
		effects_.end()
	);
}

void ParticleManager::RemoveFinishedEffects()
{
	effects_.erase(
		std::remove_if(effects_.begin(), effects_.end(),
			[](const std::unique_ptr<ParticleEffect>& e) {
				return e->IsFinished() && e->IsAutoRemove();
			}),
		effects_.end()
	);
}
