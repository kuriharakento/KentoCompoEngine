#pragma once
#include <functional>

#include "math/Vector3.h"
#include "math/Vector4.h"

/**
 * 点光源
 */
struct GPUPointLight
{
	Vector4 color;				// ライトの色
	Vector3 position;			// ライトの位置
	float intensity;			// ライトの強さ
	float radius;				// ライトの届く最大距離
	float decay;				// ライトの減衰率
};

struct CPUPointLight {
    GPUPointLight gpuData;

    // 補間用のメンバを追加
    Vector4 startColor;
    Vector4 endColor;
    float duration;       // 補間にかける時間
    float elapsedTime;    // 経過時間
    bool isReversing;     // 補間の方向
    bool isGradientActive; // グラデーションが有効かどうか
    std::function<float(float)> easingFunction; // イージング関数
};