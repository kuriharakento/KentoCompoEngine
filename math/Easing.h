#pragma once
#include <cmath>
#include <numbers>
#include "math/Vector2.h"

// 参照したサイト
// https://easings.net/ja

//======================================* イージングの式 *=======================================//
// イージングしたい値 = 始めたい位置 + イージング関数(時間) * 始めたい位置と終わりたい位置の差分 //
//===============================================================================================//

/// 半分の値（0.5）
constexpr float kHalfValue = 0.5f;
/// 2倍の値
constexpr float kDoubleValue = 2.0f;
/// Back系イージングの制御定数 c1
constexpr float kBackC1 = 1.70158f;
/// Back系イージングの制御定数 c2 の乗数
constexpr float kBackC2Multiplier = 1.525f;
/// Bounce系イージングの反発係数 n1
constexpr float kBounceN1 = 7.5625f;
/// Bounce系イージングの区間分割定数 d1
constexpr float kBounceD1 = 2.75f;
/// Bounce系イージングの区間オフセット1
constexpr float kBounceOffset1 = 1.5f;
/// Bounce系イージングの区間オフセット2
constexpr float kBounceOffset2 = 2.25f;
/// Bounce系イージングの区間オフセット3
constexpr float kBounceOffset3 = 2.625f;
/// Bounce系イージングの高さ補正1
constexpr float kBounceHeight1 = 0.75f;
/// Bounce系イージングの高さ補正2
constexpr float kBounceHeight2 = 0.9375f;
/// Bounce系イージングの高さ補正3
constexpr float kBounceHeight3 = 0.984375f;
/// Bounce系イージングの区間境界2
constexpr float kBounceBoundary2 = 2.5f;
/// Quint系イージングの5乗係数（2^4 = 16）
constexpr float kQuintCoefficient = 16.0f;
/// Quart系イージングの4乗係数（2^3 = 8）
constexpr float kQuartCoefficient = 8.0f;
/// Elastic系イージングのオフセット（10.75）
constexpr float kElasticOffset1 = 10.75f;
/// Elastic系イージングのオフセット（0.75）
constexpr float kElasticOffset2 = 0.75f;
/// Elastic系イージングのオフセット（11.125）
constexpr float kElasticOffset3 = 11.125f;
/// Elastic/Expo系イージングの指数計算用オフセット
constexpr float kExponentOffset = 10.0f;
/// Elastic/Expo系イージングの乗数（20）
constexpr float kExponentMultiplier20 = 20.0f;
/// 指数（5）Quint用
constexpr int kPowerFive = 5;
/// 指数（4）Quart用
constexpr int kPowerFour = 4;

/**
 * @brief 指定したイージング関数を使用して、開始値から終了値までの間の補間値を計算
 * @tparam T 値の型（浮動小数点数や整数など）
 * @param start イージングを始めたい位置（または値）
 * @param end イージングを終えたい位置（または値）
 * @param func イージング関数 (例: EaseInSine, EaseOutQuad など)
 * @param progress イージングの進行具合。0.0f は開始点、1.0f は終了点に対応。0.0f 〜 1.0f の範囲で指定。
 * @return 開始位置から終了位置までの間の値を返します。
 **/
template<class T> T EasingToEnd(T start, T end, T (*func)(T), T progress) { return start + func(progress) * (end - start); }

/**
 * @brief 指定したイージング関数を使用して、開始値から終了値までの間の補間値を計算
 * @tparam T 値の型（浮動小数点数や整数など）
 * @param start イージングを始めたい位置（または値）
 * @param amount 動かしたい量（開始位置からの移動距離や変化量）
 * @param func イージング関数 (例: EaseInSine, EaseOutQuad など)
 * @param progress イージングの進行具合。0.0f は開始点、1.0f は終了点に対応。0.0f 〜 1.0f の範囲で指定。
 * @return 開始位置から動かしたい量に応じた補間値を返します。
 **/
template<class T> T EasingByAmout(T start, T amount, T (*func)(T), T progress) { return start + func(progress) * amount; }

//============================================
// Sine - 正弦波に基づくイージング
//============================================

/**
 * @brief EaseInSine - 正弦波で徐々に加速
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseInSine(T x) { return static_cast<T>(1 - cos((x * std::numbers::pi_v<T>) / kDoubleValue)); }

/**
 * @brief EaseOutSine - 正弦波で徐々に減速
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseOutSine(T x) { return static_cast<T>(sin((x * std::numbers::pi_v<T>) / kDoubleValue)); }

/**
 * @brief EaseInOutSine - 正弦波で加速後減速
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseInOutSine(T x) { return static_cast<T>(-(cos(std::numbers::pi_v<T> * x) - 1) / kDoubleValue); }

//============================================
// Quint - 5乗に基づくイージング
//============================================

/**
 * @brief EaseInQuint - 5乗で徐々に加速（x^5）
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseInQuint(T x) { return x * x * x * x * x; }

/**
 * @brief EaseOutQuint - 5乗で徐々に減速
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseOutQuint(T x) { return static_cast<T>(1 - pow(1 - x, kPowerFive)); }

/**
 * @brief EaseInOutQuint - 5乗で加速後減速
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseInOutQuint(T x) { return x < kHalfValue ? static_cast<T>(kQuintCoefficient * x * x * x * x * x) : static_cast<T>(1 - pow(-kDoubleValue * x + kDoubleValue, kPowerFive) / kDoubleValue); }

//============================================
// Circ - 円に基づくイージング
//============================================

/**
 * @brief EaseInCirc - 円弧で徐々に加速
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseInCirc(T x) { return static_cast<T>(1 - sqrt(1 - pow(x, kDoubleValue))); }

/**
 * @brief EaseOutCirc - 円弧で徐々に減速
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseOutCirc(T x) { return static_cast<T>(sqrt(1 - pow(x - 1, kDoubleValue))); }

/**
 * @brief EaseInOutCirc - 円弧で加速後減速
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseInOutCirc(T x) { return x < kHalfValue ? static_cast<T>((1 - sqrt(1 - pow(kDoubleValue * x, kDoubleValue))) / kDoubleValue) : static_cast<T>((sqrt(1 - pow(-kDoubleValue * x + kDoubleValue, kDoubleValue)) + 1) / kDoubleValue); }

//============================================
// Elastic - 弾性（バネ）に基づくイージング
//============================================

/**
 * @brief EaseInElastic - 弾性で徐々に加速（バネのような動き）
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseInElastic(T x) {
	const T c4 = static_cast<T>((kDoubleValue * std::numbers::pi_v<T>) / 3);

	return x == 0 ? 0 : x == 1 ? 1 : static_cast<T>(-pow(kDoubleValue, kExponentOffset * x - kExponentOffset) * sin((x * kExponentOffset - kElasticOffset1) * c4));
}

/**
 * @brief EaseOutElastic - 弾性で徐々に減速（バネのような動き）
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseOutElastic(T x) {
	const T c4 = static_cast<T>((kDoubleValue * std::numbers::pi_v<T>) / 3);

	return x == 0 ? 0 : x == 1 ? 1 : static_cast<T>(pow(kDoubleValue, -kExponentOffset * x) * sin((x * kExponentOffset - kElasticOffset2) * c4) + 1);
}

/**
 * @brief EaseInOutElastic - 弾性で加速後減速（バネのような動き）
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseInOutElastic(T x) {
	const T c5 = static_cast<T>((kDoubleValue * std::numbers::pi_v<T>) / 4.5f);
	return x == 0 ? 0 : x == 1 ? 1 : x < kHalfValue ? static_cast<T>(-(pow(kDoubleValue, kExponentMultiplier20 * x - kExponentOffset) * sin((kExponentMultiplier20 * x - kElasticOffset3) * c5)) / kDoubleValue) : static_cast<T>((pow(kDoubleValue, -kExponentMultiplier20 * x + kExponentOffset) * sin((kExponentMultiplier20 * x - kElasticOffset3) * c5)) / kDoubleValue + 1);
}

//============================================
// Expo - 指数に基づくイージング
//============================================

/**
 * @brief EaseInExpo - 指数関数で徐々に加速（2^10x）
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseInExpo(T x) { return x == 0 ? 0 : static_cast<T>(pow(kDoubleValue, kExponentOffset * x - kExponentOffset)); }

/**
 * @brief EaseOutExpo - 指数関数で徐々に減速
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseOutExpo(T x) { return x == 1 ? 1 : static_cast<T>(1 - powf(kDoubleValue, -kExponentOffset * x)); }

/**
 * @brief EaseInOutExpo - 指数関数で加速後減速
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template <class T> T EaseInOutExpo(T x)
{
	if (x == 0) return 0;
	if (x == 1) return 1;
	return x < kHalfValue ? static_cast<T>(pow(kDoubleValue, kExponentMultiplier20 * x - kExponentOffset) / kDoubleValue) : static_cast<T>((kDoubleValue - pow(kDoubleValue, -kExponentMultiplier20 * x + kExponentOffset)) / kDoubleValue);
}

//============================================
// Quad - 2乗に基づくイージング
//============================================

/**
 * @brief EaseInQuad - 2乗で徐々に加速
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseInQuad(T x) { return x * x; }

/**
 * @brief EaseOutQuad - 2乗で徐々に減速（1 - (1-x)^2）
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseOutQuad(T x) { return static_cast<T>(1 - (1 - x) * (1 - x)); }

/**
 * @brief EaseInOutQuad - 2乗で加速後減速
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseInOutQuad(T x) { return x < kHalfValue ? static_cast<T>(kDoubleValue * x * x) : static_cast<T>(1 - pow(-kDoubleValue * x + kDoubleValue, kDoubleValue) / kDoubleValue); }

//============================================
// Cubic - 3乗に基づくイージング
//============================================

/**
 * @brief EaseInCubic - 3乗で徐々に加速
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseInCubic(T x) { return x * x * x; }

/**
 * @brief EaseOutCubic - 3乗で徐々に減速
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseOutCubic(T x) { return static_cast<T>(1 - pow(1 - x, 3)); }

/**
 * @brief EaseInOutCubic - 3乗で加速後減速
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseInOutCubic(T x) { return x < kHalfValue ? static_cast<T>(4 * x * x * x) : static_cast<T>(1 - pow(-kDoubleValue * x + kDoubleValue, 3) / kDoubleValue); }

//============================================
// Quart - 4乗に基づくイージング
//============================================

/**
 * @brief EaseInQuart - 4乗で徐々に加速
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseInQuart(T x) { return x * x * x * x; }

/**
 * @brief EaseOutQuart - 4乗で徐々に減速
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseOutQuart(T x) { return static_cast<T>(1 - pow(1 - x, kPowerFour)); }

/**
 * @brief EaseInOutQuart - 4乗で加速後減速
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseInOutQuart(T x) { return x < kHalfValue ? static_cast<T>(kQuartCoefficient * x * x * x * x) : static_cast<T>(1 - pow(-kDoubleValue * x + kDoubleValue, kPowerFour) / kDoubleValue); }

//============================================
// Back - オーバーシュートを伴うイージング
//============================================

/**
 * @brief EaseInBack - 開始時に少し戻ってから加速（オーバーシュート）
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseInBack(T x) {
	const T c1 = static_cast<T>(kBackC1);
	const T c3 = c1 + 1;
	return static_cast<T>(c3 * x * x * x - c1 * x * x);
}

/**
 * @brief EaseOutBack - 終了時に少し行き過ぎてから減速（オーバーシュート）
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseOutBack(T x) {
	const T c1 = static_cast<T>(kBackC1);
	const T c3 = c1 + 1;
	return static_cast<T>(1 + c3 * pow(x - 1, 3) + c1 * pow(x - 1, kDoubleValue));
}

/**
 * @brief EaseInOutBack - 開始・終了時にオーバーシュート
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseInOutBack(T x) {
	const T c1 = static_cast<T>(kBackC1);
	const T c2 = c1 * static_cast<T>(kBackC2Multiplier);
	return x < kHalfValue ? static_cast<T>((pow(kDoubleValue * x, kDoubleValue) * ((c2 + 1) * kDoubleValue * x - c2)) / kDoubleValue) : static_cast<T>((pow(kDoubleValue * x - kDoubleValue, kDoubleValue) * ((c2 + 1) * (x * kDoubleValue - kDoubleValue) + c2) + kDoubleValue) / kDoubleValue);
}

//============================================
// Bounce - バウンド（跳ね返り）イージング
//============================================

/**
 * @brief EaseOutBounce - 終了時にバウンドしながら減速
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseOutBounce(T x); // 前方宣言

/**
 * @brief EaseInBounce - 開始時にバウンドしながら加速
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseInBounce(T x) { return static_cast<T>(1 - EaseOutBounce(1 - x)); }

template<class T> T EaseOutBounce(T x) {
	const T n1 = static_cast<T>(kBounceN1);
	const T d1 = static_cast<T>(kBounceD1);
	if (x < 1 / d1) {
		return static_cast<T>(n1 * x * x);
	} else if (x < kDoubleValue / d1) {
		return static_cast<T>(n1 * (x -= static_cast<T>(kBounceOffset1) / d1) * x + static_cast<T>(kBounceHeight1));
	} else if (x < kBounceBoundary2 / d1) {
		return static_cast<T>(n1 * (x -= static_cast<T>(kBounceOffset2) / d1) * x + static_cast<T>(kBounceHeight2));
	} else {
		return static_cast<T>(n1 * (x -= static_cast<T>(kBounceOffset3) / d1) * x + static_cast<T>(kBounceHeight3));
	}
}

/**
 * @brief EaseInOutBounce - 開始・終了時にバウンド
 * @tparam T 値の型
 * @param x 進行度（0.0〜1.0）
 * @return イージング適用後の値
 */
template<class T> T EaseInOutBounce(T x) { return x < kHalfValue ? static_cast<T>((1 - EaseOutBounce(1 - kDoubleValue * x)) / kDoubleValue) : static_cast<T>((1 + EaseOutBounce(kDoubleValue * x - 1)) / kDoubleValue); }


//============================================
// LerpAngle - 角度の線形補間
//============================================

/**
 * @brief 角度の線形補間（最短経路を使用）
 * @tparam T 値の型
 * @param a 開始角度（ラジアン）
 * @param b 終了角度（ラジアン）
 * @param t 補間係数（0.0〜1.0）
 * @return 補間された角度
 */
template<class T> T LerpAngle(T a, T b, T t) {
	T diff = b - a;
	while (diff > std::numbers::pi_v<float>) {
		diff -= kDoubleValue * std::numbers::pi_v<float>;
	}

	while (diff < -std::numbers::pi_v<float>) {
		diff += kDoubleValue * std::numbers::pi_v<float>;
	}
	return a + diff * t;
}