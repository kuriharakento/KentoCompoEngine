#pragma once
#include <cmath>
#include <numbers>
#include "math/Vector2.h"

// 参照したサイト
// https://easings.net/ja

//======================================* イージングの式 *=======================================//
// イージングしたい値 = 始めたい位置 + イージング関数(時間) * 始めたい位置と終わりたい位置の差分 //
//===============================================================================================//

/**
 * \brief 指定したイージング関数を使用して、開始値から終了値までの間の補間値を計算
 * \tparam T 値の型（浮動小数点数や整数など）
 * \param start イージングを始めたい位置（または値）
 * \param end イージングを終えたい位置（または値）
 * \param func イージング関数 (例: EaseInSine, EaseOutQuad など)
 * \param progress イージングの進行具合。0.0f は開始点、1.0f は終了点に対応。0.0f 〜 1.0f の範囲で指定。
 * \return 開始位置から終了位置までの間の値を返します。
 **/
template<class T> T EasingToEnd(T start, T end, T (*func)(T), T progress) { return start + func(progress) * (end - start); }

/**
 * \brief 指定したイージング関数を使用して、開始値から終了値までの間の補間値を計算
 * \tparam T 値の型（浮動小数点数や整数など）
 * \param start イージングを始めたい位置（または値）
 * \param amount 動かしたい量（開始位置からの移動距離や変化量）
 * \param func イージング関数 (例: EaseInSine, EaseOutQuad など)
 * \param progress イージングの進行具合。0.0f は開始点、1.0f は終了点に対応。0.0f 〜 1.0f の範囲で指定。
 * \return 開始位置から動かしたい量に応じた補間値を返します。
 **/
template<class T> T EasingByAmout(T start, T amount, T (*func)(T), T progress) { return start + func(progress) * amount; }

//============================================
// Sine
//============================================

template<class T> T EaseInSine(T x) { return static_cast<T>(1 - cos((x * std::numbers::pi_v<T>) / 2)); }

template<class T> T EaseOutSine(T x) { return static_cast<T>(sin((x * std::numbers::pi_v<T>) / 2)); }

template<class T> T EaseInOutSine(T x) { return static_cast<T>(-(cos(std::numbers::pi_v<T> * x) - 1) / 2); }

//============================================
// Quint
//============================================

template<class T> T EaseInQuint(T x) { return x * x * x * x * x; }

template<class T> T EaseOutQuint(T x) { return static_cast<T>(1 - pow(1 - x, 5)); }

template<class T> T EaseInOutQuint(T x) { return x < 0.5 ? static_cast<T>(16 * x * x * x * x * x) : static_cast<T>(1 - pow(-2 * x + 2, 5) / 2); }

//============================================
// Circ
//============================================

template<class T> T EaseInCirc(T x) { return static_cast<T>(1 - sqrt(1 - pow(x, 2))); }

template<class T> T EaseOutCirc(T x) { return static_cast<T>(sqrt(1 - pow(x - 1, 2))); }

template<class T> T EaseInOutCirc(T x) { return x < 0.5 ? static_cast<T>((1 - sqrt(1 - pow(2 * x, 2))) / 2) : static_cast<T>((sqrt(1 - pow(-2 * x + 2, 2)) + 1) / 2); }

//============================================
// Elastic
//============================================

template<class T> T EaseInElastic(T x) {
	const T c4 = static_cast<T>((2 * std::numbers::pi_v<T>) / 3);

	return x == 0 ? 0 : x == 1 ? 1 : static_cast<T>(-pow(2, 10 * x - 10) * sin((x * 10 - 10.75) * c4));
}

template<class T> T EaseOutElastic(T x) {
	const T c4 = static_cast<T>((2 * std::numbers::pi_v<T>) / 3);

	return x == 0 ? 0 : x == 1 ? 1 : static_cast<T>(pow(2, -10 * x) * sin((x * 10 - 0.75) * c4) + 1);
}

template<class T> T EaseInOutElastic(T x) {
	const T c5 = static_cast<T>((2 * std::numbers::pi_v<T>) / 4.5);
	return x == 0 ? 0 : x == 1 ? 1 : x < 0.5 ? static_cast<T>(-(pow(2, 20 * x - 10) * sin((20 * x - 11.125) * c5)) / 2) : static_cast<T>((pow(2, -20 * x + 10) * sin((20 * x - 11.125) * c5)) / 2 + 1);
}

//============================================
// Expo
//============================================

template<class T> T EaseInExpo(T x) { return x == 0 ? 0 : static_cast<T>(pow(2, 10 * x - 10)); }

template<class T> T EaseOutExpo(T x) { return x == 1 ? 1 : static_cast<T>(1 - powf(2, -10 * x)); }


//============================================
// Quad
//============================================

template<class T> T EaseOutQuad(T x) { return static_cast<T>(1 - (1 - x) * (1 - x)); }

//============================================
// Quart
//============================================

template<class T> T EaseInOutQuart(T x) { return x < 0.5 ? static_cast<T>(8 * x * x * x * x) : static_cast<T>(1 - pow(-2 * x + 2, 4) / 2); }

//============================================
// Back
//============================================

template<class T> T EaseInBack(T x) {
	const T c1 = static_cast<T>(1.70158);
	const T c3 = c1 + 1;
	return static_cast<T>(c3 * x * x * x - c1 * x * x);
}

template<class T> T EaseOutBack(T x) {
	const T c1 = static_cast<T>(1.70158);
	const T c3 = c1 + 1;
	return static_cast<T>(1 + c3 * pow(x - 1, 3) + c1 * pow(x - 1, 2));
}

template<class T> T EaseInOutBack(T x) {
	const T c1 = static_cast<T>(1.70158);
	const T c2 = c1 * static_cast<T>(1.525);
	return x < 0.5 ? static_cast<T>((pow(2 * x, 2) * ((c2 + 1) * 2 * x - c2)) / 2) : static_cast<T>((pow(2 * x - 2, 2) * ((c2 + 1) * (x * 2 - 2) + c2) + 2) / 2);
}

//============================================
// Bounce
//============================================

template<class T> T EaseOutBounce(T x); // 前方宣言

template<class T> T EaseInBounce(T x) { return static_cast<T>(1 - EaseOutBounce(1 - x)); }

template<class T> T EaseOutBounce(T x) {
	const T n1 = static_cast<T>(7.5625);
	const T d1 = static_cast<T>(2.75);
	if (x < 1 / d1) {
		return static_cast<T>(n1 * x * x);
	} else if (x < 2 / d1) {
		return static_cast<T>(n1 * (x -= static_cast<T>(1.5) / d1) * x + static_cast<T>(0.75));
	} else if (x < 2.5 / d1) {
		return static_cast<T>(n1 * (x -= static_cast<T>(2.25) / d1) * x + static_cast<T>(0.9375));
	} else {
		return static_cast<T>(n1 * (x -= static_cast<T>(2.625) / d1) * x + static_cast<T>(0.984375));
	}
}

template<class T> T EaseInOutBounce(T x) { return x < 0.5 ? static_cast<T>((1 - EaseOutBounce(1 - 2 * x)) / 2) : static_cast<T>((1 + EaseOutBounce(2 * x - 1)) / 2); }


//============================================
// LerpAngle
//============================================

template<class T> T LerpAngle(T a, T b, T t) {
	T diff = b - a;
	while (diff > std::numbers::pi_v<float>) {
		diff -= 2.0f * std::numbers::pi_v<float>;
	}

	while (diff < -std::numbers::pi_v<float>) {
		diff += 2.0f * std::numbers::pi_v<float>;
	}
	return a + diff * t;
}