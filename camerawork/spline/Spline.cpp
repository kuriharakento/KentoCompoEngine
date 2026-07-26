#include "Spline.h"

namespace KCE
{
// Catmull-Rom係数（スプライン補間の基本係数）
constexpr float kCatmullRomHalf = 0.5f;
// P1の係数
constexpr float kCatmullRomP1Coeff = 2.0f;
// t^2項のP0係数
constexpr float kCatmullRomT2P0Coeff = 2.0f;
// t^2項のP1係数
constexpr float kCatmullRomT2P1Coeff = 5.0f;
// t^2項のP2係数
constexpr float kCatmullRomT2P2Coeff = 4.0f;
// t^3項のP1/P2係数
constexpr float kCatmullRomT3P1P2Coeff = 3.0f;

Vector3 Spline::CatmullRom(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t)
{
    // tの累乗を事前計算
    float t2 = t * t;
    float t3 = t2 * t;

    // Catmull-Rom補間の計算式:
    // P(t) = 0.5 * ((2*P1) + (-P0+P2)*t + (2*P0-5*P1+4*P2-P3)*t^2 + (-P0+3*P1-3*P2+P3)*t^3)
    auto calc = [&](float p0_, float p1_, float p2_, float p3_) {
        return kCatmullRomHalf * (
            (kCatmullRomP1Coeff * p1_) + 
            (-p0_ + p2_) * t + 
            (kCatmullRomT2P0Coeff * p0_ - kCatmullRomT2P1Coeff * p1_ + kCatmullRomT2P2Coeff * p2_ - p3_) * t2 + 
            (-p0_ + kCatmullRomT3P1P2Coeff * p1_ - kCatmullRomT3P1P2Coeff * p2_ + p3_) * t3
        );
    };

    // 各軸に対して補間を計算
    return Vector3(
        calc(p0.x, p1.x, p2.x, p3.x),
        calc(p0.y, p1.y, p2.y, p3.y),
        calc(p0.z, p1.z, p2.z, p3.z)
    );
}
} // namespace KCE
