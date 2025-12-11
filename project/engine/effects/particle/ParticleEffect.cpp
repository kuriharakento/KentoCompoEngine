#include "ParticleEffect.h"
#include "ParticleEmitter.h"
#include "serialization/ParticleEffectSerializer.h"
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
	if (!isPlaying_) return;

	for (auto& emitter : emitters_)
	{
		emitter->Update(deltaTime, camera);
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
}

void ParticleEffect::Stop()
{
	isPlaying_ = false;
}

void ParticleEffect::Reset()
{
	for (auto& emitter : emitters_)
	{
		emitter->GetParticles().clear();
	}
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
