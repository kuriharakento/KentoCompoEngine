#pragma once
#include "MatrixFunc.h"
#include "math/Vector3.h"

struct OBB
{
	Vector3 center;		// 中心座標
	Matrix4x4 rotate;	// 回転行列
	Vector3 size;		// サイズ
	OBB() : center(), rotate(), size() {}
};