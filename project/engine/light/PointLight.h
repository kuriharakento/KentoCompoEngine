#pragma once
#include <functional>

#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/MatrixFunc.h"

/**
 * @brief GPU用ポイントライト構造体
 * 
 * シェーダーに送信するポイントライト（点光源）のデータ。
 * 特定の位置から全方向に光を放射する光源を表現します。
 * 距離に応じた減衰を持ち、電球やランプなどの表現に適しています。
 */
struct GPUPointLight
{
	Vector4 color;				// ライトの色（RGBAで指定）
	Vector3 position;			// ライトの位置（ワールド座標）
	float intensity;			// ライトの強さ（明るさの倍率）
	float radius;				// ライトの届く最大距離
	float decay;				// ライトの減衰率（高いほど早く減衰）
};

/**
 * @brief CPU用ポイントライト構造体
 * 
 * GPU側のデータに加え、アニメーションや補間処理用のメンバを持つ
 * CPU側で管理するポイントライト。色のグラデーションなど
 * 時間経過による変化を実装可能。
 */
struct CPUPointLight {
    GPUPointLight gpuData;   // GPU送信用データ

    Vector4 startColor;      // グラデーション開始色
    Vector4 endColor;        // グラデーション終了色
    float duration;          // 補間にかける時間（秒）
    float elapsedTime;       // 経過時間（秒）
    bool isReversing;        // 補間の方向（往復用）
    bool isGradientActive;   // グラデーションが有効かどうか
    std::function<float(float)> easingFunction; // イージング関数

    // キューブマップシャドウ用（6方向: +X, -X, +Y, -Y, +Z, -Z）
    Matrix4x4 viewMatrices[6];           // ライトビュー行列（6面）
    Matrix4x4 projectionMatrix;          // ライトプロジェクション行列（共通）
    Matrix4x4 viewProjectionMatrices[6]; // ビュー・プロジェクション行列（6面）
    bool shadowEnabled = false;          // シャドウ有効フラグ
};
