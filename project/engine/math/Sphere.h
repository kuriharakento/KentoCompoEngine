#pragma once
#include "Vector3.h"

struct Sphere
{
    Vector3 center;
    float radius;

    Sphere() : center(0, 0, 0), radius(1.0f) {}
    Sphere(const Vector3& c, float r) : center(c), radius(r) {}
};
