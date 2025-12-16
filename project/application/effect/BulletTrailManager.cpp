#include "BulletTrailManager.h"
#include "effects/particle/ParticleManager.h"
#include "effects/particle/ParticleEffect.h"

BulletTrailManager& BulletTrailManager::GetInstance()
{
	static BulletTrailManager instance;
	return instance;
}

void BulletTrailManager::Initialize()
{
	if (initialized_) return;

	// JSONからエフェクト定義を読み込み
	ParticleManager::GetInstance()->LoadEffectDefinition(kEffectName, kEffectJsonPath);

	initialized_ = true;
}

uint32_t BulletTrailManager::RegisterBullet(Transform* bulletTransform)
{
	if (!initialized_) Initialize();

	// エフェクトを再生（Transform追従）
	ParticleEffect* effect = ParticleManager::GetInstance()->Play(kEffectName, bulletTransform);
	
	if (!effect) return 0;

	// IDを割り当てて管理
	uint32_t trailId = nextId_++;
	activeTrails_[trailId] = effect;

	return trailId;
}

void BulletTrailManager::UnregisterBullet(uint32_t trailId)
{
	auto it = activeTrails_.find(trailId);
	if (it != activeTrails_.end())
	{
		// エフェクトを停止
		if (it->second)
		{
			it->second->Stop();
			ParticleManager::GetInstance()->RemoveEffect(it->second);
		}
		activeTrails_.erase(it);
	}
}

void BulletTrailManager::Clear()
{
	// 全エフェクトを停止
	for (auto& [id, effect] : activeTrails_)
	{
		if (effect)
		{
			effect->Stop();
			ParticleManager::GetInstance()->RemoveEffect(effect);
		}
	}
	activeTrails_.clear();
}
