#pragma once
#include "application/GameObject/Combatable/character/player/Player.h"
#include "effects/particle/ParticleEmitter.h"

/**
 * @brief 繝励Ξ繧､繝､繝ｼ豁ｻ莠｡譎ゅ・繝薙ず繝･繧｢繝ｫ繧ｨ繝輔ぉ繧ｯ繝医け繝ｩ繧ｹ・・PS迚茨ｼ・
 */
class PlayerDeathEffect
{
public:
	void Initialize(Player* player);
	void Update();
	void Play(float duration);
	bool IsFinished() const;

private:
	Player* player_ = nullptr;
	Vector3 initialScale_;
	bool isParticleStarted_ = false;
	bool isActive_ = false;
	float elapsed_ = 0.0f;
	float duration_ = 1.0f;
	std::string emitterName_ = "player_death_debris";
};

