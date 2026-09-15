#include "ParticleManager.h"
#include "ParticleEffect.h"
#include "base/DirectXCommon.h"
#include "manager/system/SrvManager.h"
#include "manager/scene/CameraManager.h"
#include "manager/effect/ParticlePipelineManager.h"
#include "effects/particle/renderer/IRenderer.h"
#include "time/TimeManager.h"
#include "time/Timer.h"
#include "editor/SceneGizmo.h"
#include "editor/command/CommandHistory.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "editor/SelectionContext.h"
#include "manager/editor/DebugUIManager.h"

#endif

namespace KCE
{
namespace
{
class ParticlePositionCommand final : public ICommand
{
public:
	ParticlePositionCommand(ParticleManager* manager, std::string name, const Vector3& before, const Vector3& after, uint32_t dragId)
		: manager_(manager), name_(std::move(name)), before_(before), after_(after), dragId_(dragId) {}
	void Execute() override { if (manager_) { manager_->SetDebugPosition(name_, after_); } }
	void Undo() override { if (manager_) { manager_->SetDebugPosition(name_, before_); } }
	std::string GetName() const override { return "Move Particle Effect"; }
	bool MergeWith(const ICommand* next) override
	{
		const auto* command = dynamic_cast<const ParticlePositionCommand*>(next);
		if (!command || command->manager_ != manager_ || command->name_ != name_ || command->dragId_ != dragId_) { return false; }
		after_ = command->after_;
		return true;
	}
private:
	// シングルトンは履歴より長生きする
	ParticleManager* manager_ = nullptr;
	std::string name_;
	Vector3 before_{};
	Vector3 after_{};
	uint32_t dragId_ = 0;
};
}

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
	DebugUIManager::GetInstance()->RegisterSettingsPage(this, "エフェクト", "パーティクルマネージャー", [this]() { this->DrawSettingsImGui(); });
	DebugUIManager::GetInstance()->RegisterHierarchySection(this, "エフェクト", [this]() { this->DrawHierarchyImGui(); });
	DebugUIManager::GetInstance()->RegisterInspector(this, SelectionKind::ParticleEffect,
		[this](const SelectionItem& item) { this->DrawInspectorImGui(item); });
	GizmoTarget target;
	target.getPose = [this](const SelectionItem& item, Matrix4x4& world, uint32_t& operations)
	{
		Vector3 position;
		if (!GetDebugPosition(item.name, position)) { return false; }
		world = MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, {}, position);
		operations = kGizmoTranslate;
		return true;
	};
	target.apply = [this](const SelectionItem& item, const GizmoResult& after, uint32_t dragId)
	{
		Vector3 before;
		if (!GetDebugPosition(item.name, before)) { return; }
		CommandHistory::GetInstance()->Execute(std::make_unique<ParticlePositionCommand>(this, item.name, before, after.translate, dragId));
	};
	SceneGizmo::GetInstance()->RegisterTarget(this, SelectionKind::ParticleEffect, std::move(target));
#endif
}

void ParticleManager::Finalize()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->Unregister(this);
	}
	if (SceneGizmo::HasInstance()) { SceneGizmo::GetInstance()->Unregister(this); }
#endif
	effects_.clear();
	effectPools_.clear();
	rendererTrashBin_.clear();
	emitters_.clear();
	effectDefinitions_.clear();
	pipelineManager_.reset();
}

bool ParticleManager::GetDebugPosition(const std::string& name, Vector3& position) const
{
	for (const auto& effect : effects_)
	{
		if (effect->GetDebugName() == name) { position = effect->GetPosition(); return true; }
	}
	for (const auto& emitter : emitters_)
	{
		if (emitter->GetDebugName() == name) { position = emitter->GetPosition(); return true; }
	}
	return false;
}

bool ParticleManager::SetDebugPosition(const std::string& name, const Vector3& position)
{
	for (auto& effect : effects_)
	{
		if (effect->GetDebugName() == name) { effect->SetPosition(position); return true; }
	}
	for (auto& emitter : emitters_)
	{
		if (emitter->GetDebugName() == name) { emitter->SetPosition(position); return true; }
	}
	return false;
}

void ParticleManager::Update(CameraManager* camera)
{
	// 前フレームの描画が完全に終わったため、ゴミ箱内の古いレンダラーを安全に破棄する
	rendererTrashBin_.clear();

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

void ParticleManager::DrawSettingsImGui()
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

	ImGui::Text("パーティクルの合計: %u", totalParticles);
	ImGui::Text("エミッターの合計: %u", totalEmitters);
	ImGui::Text("動いているエフェクト: %d", static_cast<int>(effects_.size()));

	// SRV使用状況（Active = 実使用中、HWM = 確保した最大インデックス）
	uint32_t srvActive = srvManager_->GetActiveSRVCount();
	uint32_t srvHwm    = srvManager_->GetUseIndex();
	uint32_t srvMax    = SrvManager::kMaxSRVCount;
	ImGui::Text("SRV Active: %u / %u  (HWM: %u)", srvActive, srvMax, srvHwm);
	ImGui::Separator();
#endif
}

void ParticleManager::DrawHierarchyImGui()
{
#ifdef USE_IMGUI
	const SelectionItem& primary = SelectionContext::GetInstance()->GetPrimary();
	for (const auto& effect : effects_)
	{
		const bool isSelected = primary.kind == SelectionKind::ParticleEffect && primary.name == effect->GetDebugName();
		if (ImGui::Selectable(effect->GetDebugName().c_str(), isSelected))
		{
			SelectionItem item;
			item.kind = SelectionKind::ParticleEffect;
			item.name = effect->GetDebugName();
			SelectionContext::GetInstance()->Select(item);
		}
	}
	for (const auto& emitter : emitters_)
	{
		const bool isSelected = primary.kind == SelectionKind::ParticleEffect && primary.name == emitter->GetDebugName();
		if (ImGui::Selectable(emitter->GetDebugName().c_str(), isSelected))
		{
			SelectionItem item;
			item.kind = SelectionKind::ParticleEffect;
			item.name = emitter->GetDebugName();
			SelectionContext::GetInstance()->Select(item);
		}
	}
#endif
}

void ParticleManager::DrawInspectorImGui(const SelectionItem& item)
{
#ifdef USE_IMGUI
	for (auto& effect : effects_)
	{
		if (effect->GetDebugName() != item.name)
		{
			continue;
		}
		bool isPlaying = effect->IsPlaying();
		if (ImGui::Checkbox("再生中", &isPlaying))
		{
			if (isPlaying) effect->Play();
			else effect->Stop();
		}
		ImGui::SameLine();
		if (ImGui::Button("リセット"))
		{
			effect->Reset();
			effect->Play();
		}
		for (size_t i = 0; i < effect->GetEmitterCount(); ++i)
		{
			ParticleEmitter* emitter = effect->GetEmitter(i);
			if (emitter)
			{
				ImGui::Text("[%s] Particles: %d", emitter->GetName().c_str(), static_cast<int>(emitter->GetParticles().size()));
			}
		}
		return;
	}
	for (auto& emitter : emitters_)
	{
		if (emitter->GetDebugName() != item.name)
		{
			continue;
		}
		bool isEnabled = emitter->IsEnabled();
		if (ImGui::Checkbox("再生中", &isEnabled))
		{
			emitter->SetEnabled(isEnabled);
		}
		ImGui::SameLine();
		if (ImGui::Button("消す"))
		{
			emitter->ClearParticles();
		}
		ImGui::Text("パーティクル数: %d", static_cast<int>(emitter->GetParticles().size()));
		ImGui::Text("方式: %s", emitter->GetSimulationMode() == SimulationMode::GPU ? "GPU" : "CPU");
		return;
	}
	ImGui::TextDisabled("エフェクトが見つからない。");
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

void ParticleManager::RemoveEffect(ParticleEffect* effect)
{
	for (auto it = effects_.begin(); it != effects_.end();)
	{
		if (it->get() == effect)
		{
			std::string name = (*it)->GetName();
			(*it)->ResetForPool();
			effectPools_[name].push_back(std::move(*it));
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
			effectPools_[name].push_back(std::move(*it));
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
			effectPools_[effectName].push_back(std::move(*it));
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
		rendererTrashBin_.push_back(std::move(renderer));
	}
}
} // namespace KCE
