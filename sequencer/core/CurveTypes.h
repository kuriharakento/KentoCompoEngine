#pragma once
#include <algorithm>
#include <cstdint>

#include "effects/particle/ParticleTypes.h"

namespace KCE
{
/**
 * @brief キー間の補間方法
 */
enum class InterpolationMode
{
	Constant, //!< 次のキーまで値を保持する（ステップ）
	Linear,	  //!< 線形補間
	Bezier,	  //!< 3次ベジェによる時間再マッピング
};

/**
 * @brief CSSの cubic-bezier(x1, y1, x2, y2) と同じ制御点
 * @details P0 = (0,0), P3 = (1,1) は固定。x が単調増加であることを前提とする。
 */
struct BezierHandle
{
	float x1 = 0.25f;
	float y1 = 0.0f;
	float x2 = 0.75f;
	float y2 = 1.0f;

	bool operator==(const BezierHandle& other) const
	{
		return x1 == other.x1 && y1 == other.y1 && x2 == other.x2 && y2 == other.y2;
	}
	bool operator!=(const BezierHandle& other) const { return !(*this == other); }
};

/** @brief ベジェのニュートン法の反復回数 */
constexpr int kBezierNewtonIterations = 4;
/** @brief ニュートン法の導関数がこれより小さい場合は二分法に切り替える */
constexpr float kBezierMinSlope = 1e-4f;
/** @brief 二分法のフォールバック反復回数 */
constexpr int kBezierBisectionIterations = 12;

/**
 * @brief 3次ベジェのX成分を評価する
 * @param p ベジェパラメータ（0.0〜1.0）
 * @param x1 制御点1のX
 * @param x2 制御点2のX
 * @return X座標
 */
inline float BezierEvalX(float p, float x1, float x2)
{
	const float u = 1.0f - p;
	// P0.x = 0, P3.x = 1 なので第1項は消える
	return 3.0f * u * u * p * x1 + 3.0f * u * p * p * x2 + p * p * p;
}

/**
 * @brief 3次ベジェのX成分の導関数を評価する
 * @param p ベジェパラメータ（0.0〜1.0）
 * @param x1 制御点1のX
 * @param x2 制御点2のX
 * @return dX/dp
 */
inline float BezierEvalDerivativeX(float p, float x1, float x2)
{
	const float u = 1.0f - p;
	return 3.0f * u * u * x1 + 6.0f * u * p * (x2 - x1) + 3.0f * p * p * (1.0f - x2);
}

/**
 * @brief 3次ベジェのY成分を評価する
 * @param p ベジェパラメータ（0.0〜1.0）
 * @param y1 制御点1のY
 * @param y2 制御点2のY
 * @return Y座標
 */
inline float BezierEvalY(float p, float y1, float y2)
{
	const float u = 1.0f - p;
	return 3.0f * u * u * p * y1 + 3.0f * u * p * p * y2 + p * p * p;
}

/**
 * @brief 正規化された進行度にベジェイージングを適用する
 * @details x が単調であることを利用し、ニュートン法で x = t となるベジェパラメータ p を求め、
 *          その p における y を返す。ニュートン法が収束しない場合は二分法にフォールバックする。
 * @param handle ベジェ制御点
 * @param t 正規化された進行度（0.0〜1.0）
 * @return イージング適用後の進行度
 */
inline float ApplyBezierEasing(const BezierHandle& handle, float t)
{
	if (t <= 0.0f) { return 0.0f; }
	if (t >= 1.0f) { return 1.0f; }

	// 線形（0,0,1,1）は解を求めるまでもない
	if (handle.x1 == handle.y1 && handle.x2 == handle.y2) { return t; }

	// ニュートン法。初期値は t 自身（x はおおむね t に近い）
	float p = t;
	for (int i = 0; i < kBezierNewtonIterations; ++i)
	{
		const float slope = BezierEvalDerivativeX(p, handle.x1, handle.x2);
		if (std::abs(slope) < kBezierMinSlope) { break; }
		const float x = BezierEvalX(p, handle.x1, handle.x2) - t;
		if (std::abs(x) < 1e-5f) { return BezierEvalY(p, handle.y1, handle.y2); }
		p -= x / slope;
	}

	// 収束しなかった場合は範囲を保証できる二分法で詰める
	if (p < 0.0f || p > 1.0f)
	{
		float low = 0.0f;
		float high = 1.0f;
		p = t;
		for (int i = 0; i < kBezierBisectionIterations; ++i)
		{
			const float x = BezierEvalX(p, handle.x1, handle.x2);
			if (x < t) { low = p; }
			else { high = p; }
			p = (low + high) * 0.5f;
		}
	}

	return BezierEvalY(p, handle.y1, handle.y2);
}

/**
 * @brief 既存の EasingType を等価なベジェ制御点に変換する
 * @details 評価経路をベジェ1本に統一するための変換表。
 *          ベジェで表現できない Elastic / Bounce / Back 系は近似となるため、
 *          正確な形状が要る箇所では ApplyEasing() を直接使うこと。
 * @param type イージングタイプ
 * @return 対応するベジェ制御点
 */
inline BezierHandle EasingTypeToBezier(EasingType type)
{
	switch (type)
	{
	case EasingType::Linear:         return { 0.0f, 0.0f, 1.0f, 1.0f };
	case EasingType::EaseInSine:     return { 0.12f, 0.0f, 0.39f, 0.0f };
	case EasingType::EaseOutSine:    return { 0.61f, 1.0f, 0.88f, 1.0f };
	case EasingType::EaseInOutSine:  return { 0.37f, 0.0f, 0.63f, 1.0f };
	case EasingType::EaseInQuad:     return { 0.11f, 0.0f, 0.5f, 0.0f };
	case EasingType::EaseOutQuad:    return { 0.5f, 1.0f, 0.89f, 1.0f };
	case EasingType::EaseInOutQuad:  return { 0.45f, 0.0f, 0.55f, 1.0f };
	case EasingType::EaseInCubic:    return { 0.32f, 0.0f, 0.67f, 0.0f };
	case EasingType::EaseOutCubic:   return { 0.33f, 1.0f, 0.68f, 1.0f };
	case EasingType::EaseInOutCubic: return { 0.65f, 0.0f, 0.35f, 1.0f };
	case EasingType::EaseInQuart:    return { 0.5f, 0.0f, 0.75f, 0.0f };
	case EasingType::EaseOutQuart:   return { 0.25f, 1.0f, 0.5f, 1.0f };
	case EasingType::EaseInOutQuart: return { 0.76f, 0.0f, 0.24f, 1.0f };
	case EasingType::EaseInQuint:    return { 0.64f, 0.0f, 0.78f, 0.0f };
	case EasingType::EaseOutQuint:   return { 0.22f, 1.0f, 0.36f, 1.0f };
	case EasingType::EaseInOutQuint: return { 0.83f, 0.0f, 0.17f, 1.0f };
	case EasingType::EaseInExpo:     return { 0.7f, 0.0f, 0.84f, 0.0f };
	case EasingType::EaseOutExpo:    return { 0.16f, 1.0f, 0.3f, 1.0f };
	case EasingType::EaseInOutExpo:  return { 0.87f, 0.0f, 0.13f, 1.0f };
	case EasingType::EaseInCirc:     return { 0.55f, 0.0f, 1.0f, 0.45f };
	case EasingType::EaseOutCirc:    return { 0.0f, 0.55f, 0.45f, 1.0f };
	case EasingType::EaseInOutCirc:  return { 0.85f, 0.0f, 0.15f, 1.0f };
	case EasingType::EaseInBack:     return { 0.36f, 0.0f, 0.66f, -0.56f };
	case EasingType::EaseOutBack:    return { 0.34f, 1.56f, 0.64f, 1.0f };
	case EasingType::EaseInOutBack:  return { 0.68f, -0.6f, 0.32f, 1.6f };
	default:                         return { 0.0f, 0.0f, 1.0f, 1.0f };
	}
}

/**
 * @brief ベジェで表現できない EasingType かどうか
 * @param type イージングタイプ
 * @return Elastic / Bounce 系なら真
 */
inline bool IsNonBezierEasing(EasingType type)
{
	switch (type)
	{
	case EasingType::EaseInElastic:
	case EasingType::EaseOutElastic:
	case EasingType::EaseInOutElastic:
	case EasingType::EaseInBounce:
	case EasingType::EaseOutBounce:
	case EasingType::EaseInOutBounce:
		return true;
	default:
		return false;
	}
}
} // namespace KCE
