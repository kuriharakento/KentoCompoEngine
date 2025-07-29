#pragma once
#include "math/Vector3.h"
#include "math/Vector4.h"

/**
 * \brief ディレクショナルライト
 */
struct DirectionalLight
{
	Vector4 color;		// ライトの色
	Vector3 direction;	// ライトの向き
	float intensity;	// ライトの強さ
};