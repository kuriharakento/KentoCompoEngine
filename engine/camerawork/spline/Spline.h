#pragma once
#include "math/Vector3.h"

class Spline
{
public:
	static Vector3 CatmullRom(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t);

};

