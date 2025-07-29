#include "Spline.h"

Vector3 Spline::CatmullRom(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t)
{
    float t2 = t * t;
    float t3 = t2 * t;

    // 各成分の計算をまとめて行う
    auto calc = [&](float p0_, float p1_, float p2_, float p3_) {
        return 0.5f * ((2.0f * p1_) + (-p0_ + p2_) * t + (2.0f * p0_ - 5.0f * p1_ + 4.0f * p2_ - p3_) * t2 + (-p0_ + 3.0f * p1_ - 3.0f * p2_ + p3_) * t3);
        };

    // 各軸に対して計算
    return Vector3(
        calc(p0.x, p1.x, p2.x, p3.x),
        calc(p0.y, p1.y, p2.y, p3.y),
        calc(p0.z, p1.z, p2.z, p3.z)
    );
}
