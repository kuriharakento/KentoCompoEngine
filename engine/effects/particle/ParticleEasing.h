#pragma once
#include "ParticleTypes.h"
#include "math/Easing.h"
#include "math/MathUtils.h"

/**
 * @brief イージングタイプに基づいて値を補間する
 * @tparam T 値の型
 * @param type イージングタイプ
 * @param start 開始値
 * @param end 終了値
 * @param progress 進行度 (0.0 - 1.0)
 * @return 補間された値
 */
template<class T>
inline T ApplyEasing(EasingType type, const T& start, const T& end, float progress)
{
	switch (type)
	{
	case EasingType::Linear:           return MathUtils::Lerp(start, end, progress);
	case EasingType::EaseInSine:       progress = EaseInSine(progress); break;
	case EasingType::EaseOutSine:      progress = EaseOutSine(progress); break;
	case EasingType::EaseInOutSine:    progress = EaseInOutSine(progress); break;
	case EasingType::EaseInQuad:       progress = EaseInQuad(progress); break;
	case EasingType::EaseOutQuad:      progress = EaseOutQuad(progress); break;
	case EasingType::EaseInOutQuad:    progress = EaseInOutQuad(progress); break;
	case EasingType::EaseInCubic:      progress = EaseInCubic(progress); break;
	case EasingType::EaseOutCubic:     progress = EaseOutCubic(progress); break;
	case EasingType::EaseInOutCubic:   progress = EaseInOutCubic(progress); break;
	case EasingType::EaseInQuart:      progress = EaseInQuart(progress); break;
	case EasingType::EaseOutQuart:     progress = EaseOutQuart(progress); break;
	case EasingType::EaseInOutQuart:   progress = EaseInOutQuart(progress); break;
	case EasingType::EaseInQuint:      progress = EaseInQuint(progress); break;
	case EasingType::EaseOutQuint:     progress = EaseOutQuint(progress); break;
	case EasingType::EaseInOutQuint:   progress = EaseInOutQuint(progress); break;
	case EasingType::EaseInExpo:       progress = EaseInExpo(progress); break;
	case EasingType::EaseOutExpo:      progress = EaseOutExpo(progress); break;
	case EasingType::EaseInOutExpo:    progress = EaseInOutExpo(progress); break;
	case EasingType::EaseInCirc:       progress = EaseInCirc(progress); break;
	case EasingType::EaseOutCirc:      progress = EaseOutCirc(progress); break;
	case EasingType::EaseInOutCirc:    progress = EaseInOutCirc(progress); break;
	case EasingType::EaseInBack:       progress = EaseInBack(progress); break;
	case EasingType::EaseOutBack:      progress = EaseOutBack(progress); break;
	case EasingType::EaseInOutBack:    progress = EaseInOutBack(progress); break;
	case EasingType::EaseInElastic:    progress = EaseInElastic(progress); break;
	case EasingType::EaseOutElastic:   progress = EaseOutElastic(progress); break;
	case EasingType::EaseInOutElastic: progress = EaseInOutElastic(progress); break;
	case EasingType::EaseInBounce:     progress = EaseInBounce(progress); break;
	case EasingType::EaseOutBounce:    progress = EaseOutBounce(progress); break;
	case EasingType::EaseInOutBounce:  progress = EaseInOutBounce(progress); break;
	default: break;
	}
	return MathUtils::Lerp(start, end, progress);
}

/**
 * @brief IDに基づいた決定論的な乱数を生成する (0.0 - 1.0)
 * @param id パーティクルID等
 * @param subSeed 追加のシード値
 * @return 0.0f 〜 1.0f の乱数
 */
inline float DeterministicRandom(uint32_t id, uint32_t subSeed = 0)
{
	uint32_t x = id + subSeed * 0x9e3779b9;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = (x >> 16) ^ x;
	return static_cast<float>(x) / static_cast<float>(0xFFFFFFFF);
}
