#pragma once
#include "math/Vector3.h"
#include "math/Vector4.h"

/**
 * @brief ディレクショナルライト（平行光源）
 * 
 * 太陽光のように無限遠から届く平行光源を表現します。
 * 位置の概念がなく、方向のみを持ちます。
 * シーン全体を均一に照らすのに適しています。
 */
struct DirectionalLight
{
	Vector4 color;		// ライトの色（RGBAで指定）
	Vector3 direction;	// ライトの向き（正規化されたベクトル）
	float intensity;	// ライトの強さ（明るさの倍率）
};