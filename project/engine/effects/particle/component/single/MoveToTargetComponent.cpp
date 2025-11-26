#include "MoveToTargetComponent.h"

#include "math/Easing.h"
#include "math/MathUtils.h"

// 補間係数の最小値
constexpr float kMinInterpolation = 0.0f;
// 補間係数の最大値
constexpr float kMaxInterpolation = 1.0f;

MoveToTargetComponent::MoveToTargetComponent(const Vector3& target, const float& speed) : target_(target), speed_(speed)
{
}

MoveToTargetComponent::MoveToTargetComponent(const Vector3* target, const float& speed) : targetPtr_(target), speed_(speed)
{
}

void MoveToTargetComponent::Update(Particle& particle)
{
	// 動的ターゲットが設定されている場合は毎フレーム座標を更新
	if (targetPtr_)
	{
		target_ = *targetPtr_;
	}
	// 寿命に基づく補間係数を計算（0〜1の範囲にクランプ）
	float t = particle.currentTime / particle.lifeTime;
	t = MathUtils::Clamp(t, kMinInterpolation, kMaxInterpolation);

	// イージング関数を使用して開始位置からターゲットへ補間移動
	particle.transform.translate.x = EasingToEnd<float>(particle.startPos.x, target_.x, EaseOutCirc<float>, t);
	particle.transform.translate.y = EasingToEnd<float>(particle.startPos.y, target_.y, EaseOutCirc<float>, t);
	particle.transform.translate.z = EasingToEnd<float>(particle.startPos.z, target_.z, EaseOutCirc<float>, t);
}