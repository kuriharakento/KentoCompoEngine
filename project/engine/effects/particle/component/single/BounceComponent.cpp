#include "BounceComponent.h"

#include "time/TimeManager.h"

// スケールの半分を計算するための係数
constexpr float kScaleHalfFactor = 0.5f;

BounceComponent::BounceComponent(float groundHeight, float restitution, float minVelocity) : groundHeight_(groundHeight), restitution_(restitution), minVelocity_(minVelocity)
{
}

void BounceComponent::Update(Particle& particle)
{
	// 次のフレームでのY座標を予測
	float nextY = particle.transform.translate.y + particle.velocity.y * (TimeManager::GetInstance().GetGameContext().deltaTime);

	// 地面に到達（現在位置が地面以上かつ次位置が地面以下）
	if (particle.transform.translate.y >= groundHeight_ && nextY < groundHeight_)
	{
		// パーティクルのスケール分だけ地面から浮かせる
		particle.transform.translate.y = groundHeight_ + particle.transform.scale.y * kScaleHalfFactor;
		// 速度を反転し、反発係数を適用
		particle.velocity.y = -particle.velocity.y * restitution_;
	}
}

