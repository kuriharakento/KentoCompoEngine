#include "ParticleEffect.h"
#include "ParticleEmitter.h"
#include "serialization/ParticleEffectSerializer.h"
#include "module/update/RibbonModules.h"
#include <algorithm>

ParticleEffect::ParticleEffect() = default;
ParticleEffect::~ParticleEffect() = default;

ParticleEffect::ParticleEffect(ParticleEffect&&) noexcept = default;
ParticleEffect& ParticleEffect::operator=(ParticleEffect&&) noexcept = default;

std::unique_ptr<ParticleEffect> ParticleEffect::LoadFromFile(const std::string& jsonPath)
{
	return ParticleEffectSerializer::Load(jsonPath);
}

void ParticleEffect::Initialize(const std::string& name)
{
	name_ = name;
}

void ParticleEffect::Update(float deltaTime, CameraManager* camera)
{
	// isPlaying_がfalseも既存パーティクルを寿命で自然に消すため更新を続ける
	bool hasActiveParticles = false;
	for (const auto& emitter : emitters_)
	{
		if (!emitter->GetParticles().empty())
		{
			hasActiveParticles = true;
			break;
		}
	}
	if (!isPlaying_ && !hasActiveParticles) return;

	// エミッター間追従の位置を先に設定
	for (auto& emitter : emitters_)
	{
		int followIdx = emitter->GetFollowEmitterIndex();
		if (followIdx >= 0 && followIdx < static_cast<int>(emitters_.size()))
		{
			// 追従対象のエミッター位置を設定
			emitter->SetFollowEmitterPosition(emitters_[followIdx]->GetPosition());
		}
		else
		{
			emitter->ClearFollowEmitterPosition();
		}
	}

	// 各エミッターを更新
	for (auto& emitter : emitters_)
	{
		emitter->Update(deltaTime, camera);
	}

	// 全エミッターの生成が終わりパーティクルも消えたら再生完了とみなす
	// （ループ系エミッターは IsEmitting() が true のままなので自動停止しない）
	if (isPlaying_)
	{
		bool allComplete = true;
		for (const auto& emitter : emitters_)
		{
			if (emitter->IsEmitting() || !emitter->GetParticles().empty())
			{
				allComplete = false;
				break;
			}
		}
		if (allComplete)
		{
			isPlaying_ = false;
		}
	}
}

void ParticleEffect::Draw(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	for (auto& emitter : emitters_)
	{
		emitter->Draw(dxCommon, srvManager);
	}
}

void ParticleEffect::AddEmitter(std::unique_ptr<ParticleEmitter> emitter)
{
	// 新しいエミッタにエフェクトの現在位置を設定
	emitter->SetPosition(position_);
	emitters_.push_back(std::move(emitter));
}

void ParticleEffect::RemoveEmitter(size_t index)
{
	if (index < emitters_.size())
	{
		emitters_.erase(emitters_.begin() + index);
	}
}

ParticleEmitter* ParticleEffect::GetEmitter(const std::string& name)
{
	auto it = std::find_if(emitters_.begin(), emitters_.end(),
		[&name](const std::unique_ptr<ParticleEmitter>& e) {
			return e->GetName() == name;
		});
	return (it != emitters_.end()) ? it->get() : nullptr;
}

ParticleEmitter* ParticleEffect::GetEmitter(size_t index)
{
	return (index < emitters_.size()) ? emitters_[index].get() : nullptr;
}

const ParticleEmitter* ParticleEffect::GetEmitter(size_t index) const
{
	return (index < emitters_.size()) ? emitters_[index].get() : nullptr;
}

void ParticleEffect::SetPosition(const Vector3& position)
{
	position_ = position;
	for (auto& emitter : emitters_)
	{
		emitter->SetPosition(position);
	}
}

void ParticleEffect::SetFollowTarget(Transform* target)
{
	for (auto& emitter : emitters_)
	{
		emitter->SetFollowTarget(target);
	}
}

void ParticleEffect::Play()
{
	isPlaying_ = true;
	// 各エミッターのライフサイクル状態をリセットして生成を再開
	for (auto& emitter : emitters_)
	{
		emitter->Restart();
	}
}

void ParticleEffect::Stop()
{
	isPlaying_ = false;
	// 各エミッターの生成を止める（既存パーティクルは寿命で自然消滅させる）
	for (auto& emitter : emitters_)
	{
		emitter->Stop();
	}
}

void ParticleEffect::Reset()
{
	// パーティクルは残したまま、エミッターのライフサイクル状態だけリセット
	for (auto& emitter : emitters_)
	{
		emitter->Restart();
	}
	isPlaying_ = false;
}

void ParticleEffect::ResetForPool()
{
	for (auto& emitter : emitters_)
	{
		emitter->Reset();
	}
	position_ = {};
	isPlaying_ = false;
}

bool ParticleEffect::IsFinished() const
{
	if (isPlaying_) return false;

	for (const auto& emitter : emitters_)
	{
		if (!emitter->GetParticles().empty())
		{
			return false;
		}
	}
	return true;
}

void ParticleEffect::SaveToFile(const std::string& jsonPath)
{
	ParticleEffectSerializer::Save(*this, jsonPath);
}

uint32_t ParticleEffect::RegisterSource(Transform* transform)
{
	for (auto& emitter : emitters_)
	{
		if (auto* module = emitter->GetModule<MultiSourceRibbonModule>())
		{
			return module->RegisterSource(transform);
		}
	}
	return 0;
}

uint32_t ParticleEffect::RegisterSourceManual()
{
	for (auto& emitter : emitters_)
	{
		if (auto* module = emitter->GetModule<MultiSourceRibbonModule>())
		{
			return module->RegisterSourceManual();
		}
	}
	return 0;
}

void ParticleEffect::UpdateSourcePosition(uint32_t sourceId, const Vector3& position)
{
	for (auto& emitter : emitters_)
	{
		if (auto* module = emitter->GetModule<MultiSourceRibbonModule>())
		{
			module->UpdateSourcePosition(sourceId, position);
		}
	}
}

void ParticleEffect::UnregisterSource(uint32_t sourceId)
{
	for (auto& emitter : emitters_)
	{
		if (auto* module = emitter->GetModule<MultiSourceRibbonModule>())
		{
			module->UnregisterSource(sourceId);
		}
	}
}
