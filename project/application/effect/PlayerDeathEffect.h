#pragma once
#include "application/GameObject/Combatable/character/player/Player.h"
#include "effects/particle/ParticleEmitter.h"

class PlayerDeathEffect
{
public:
	void Initialize(Player* player);

	void Update();

	void Play(float duration);

	bool IsFinished() const;

private:
	// プレイヤーのポインタ
	Player* player_ = nullptr;
	// 初期スケール
	Vector3 initialScale_;
	// パーティクル開始フラグ
	bool isParticleStarted_ = false;
	// アクティブ状態
	bool isActive_ = false;
	// 経過時間
	float elapsed_ = 0.0f;
	// 継続時間
	float duration_ = 1.0f;
	// 破片が飛び散る
	std::unique_ptr<ParticleEmitter> debrisEmitter_;
};

