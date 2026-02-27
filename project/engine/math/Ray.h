#pragma once
#include "Vector3.h"

/**
 * @brief 3D空間上のレイ（半直線）を表す構造体
 */
struct Ray
{
	Vector3 start;      // レイの始点
	Vector3 direction;  // レイの方向（単位ベクトルであることが望ましい）
	float length;       // レイの長さ（有効範囲）

	Ray() : start(0, 0, 0), direction(0, 0, 1), length(1.0f) {}
	Ray(const Vector3& s, const Vector3& d, float l) : start(s), direction(d), length(l) {}
};
