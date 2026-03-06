#include "ParticleManager.h"
#include "ParticleEffect.h"
#include "base/DirectXCommon.h"
#include "manager/system/SrvManager.h"
#include "manager/scene/CameraManager.h"
#include "manager/effect/ParticlePipelineManager.h"
#include "time/TimeManager.h"
#include "time/Timer.h"
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

	// ImGuiを表示
	DrawImGui();
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
	// 既に登録されているエフェクトのうち、再生終了して空いているものを探す
	for (auto& effect : effects_)
	{
		if (effect->GetName() == effectName && !effect->IsPlaying() && effect->IsFinished())
		{
			effect->SetPosition(position);
			effect->Reset();
			effect->Play();
			return effect.get();
		}
	}

	// 定義があればJSONから読み込んで新規作成
	auto it = effectDefinitions_.find(effectName);
	if (it != effectDefinitions_.end())
	{
		auto effect = ParticleEffect::LoadFromFile(it->second);
		if (!effect) return nullptr;

		effect->Initialize(effectName);
		effect->SetPosition(position);
		effect->Play();
		
		ParticleEffect* ptr = effect.get();
		effects_.push_back(std::move(effect));
		return ptr;
	}

	// 登録も定義もない場合はnullptr
	return nullptr;
}

ParticleEffect* ParticleManager::Play(const std::string& effectName, Transform* followTarget)
{
	// 既に登録されているエフェクトのうち、再生終了して空いているものを探す
	for (auto& effect : effects_)
	{
		if (effect->GetName() == effectName && !effect->IsPlaying() && effect->IsFinished())
		{
			effect->SetFollowTarget(followTarget);
			effect->Reset();
			effect->Play();
			return effect.get();
		}
	}

	// 定義があればJSONから読み込んで新規作成
	auto it = effectDefinitions_.find(effectName);
	if (it != effectDefinitions_.end())
	{
		auto effect = ParticleEffect::LoadFromFile(it->second);
		if (!effect) return nullptr;

		effect->Initialize(effectName);
		effect->SetFollowTarget(followTarget);
		effect->Play();
		
		ParticleEffect* ptr = effect.get();
		effects_.push_back(std::move(effect));
		return ptr;
	}

	// 登録も定義もない場合はnullptr
	return nullptr;
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
	auto it = std::remove_if(effects_.begin(), effects_.end(),
		[&name](const std::unique_ptr<ParticleEffect>& e) {
			return e->GetName() == name;
		});
	
	if (it != effects_.end())
	{
		effects_.erase(it, effects_.end());
		return true;
	}
	return false;
}
