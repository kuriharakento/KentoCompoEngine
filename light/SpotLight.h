#pragma once
#include <cassert>
#include <d3d12.h>
#include <wrl.h>
#include <functional>

#include "DirectXTex/d3dx12.h"
#include "math/MatrixFunc.h"

namespace KCE
{
// シャドウマップの解像度
constexpr UINT kShadowMapResolution = 1024;
// シャドウマップのクリア時深度値
constexpr float kShadowMapClearDepth = 1.0f;

/**
 * @brief GPU用スポットライト構造体
 * 
 * シェーダーに送信するスポットライトのデータ。
 * 特定の位置から円錐状に光を放射する光源を表現します。
 * 懐中電灯やステージライトなどの表現に適しています。
 */
struct GPUSpotLight
{
	Vector4 color;				// ライトの色（RGBAで指定）
	Vector3 position;			// ライトの位置（ワールド座標）
	float intensity;			// ライトの強さ（明るさの倍率）
	Vector3 direction;			// ライトの向き（正規化されたベクトル）
	float distance;				// ライトの届く最大距離
	float decay;				// ライトの減衰率（高いほど早く減衰）
	float cosAngle;				// ライト円錐角度の余弦（内側境界）
	float cosFalloffStart;		// フォールオフ開始角度の余弦（外側境界）
	int32_t shadowEnabled;		// シャドウ有効フラグ（0:無効、1:有効）
	Vector4 padding;			// 16バイトアライメント用パディング
	Matrix4x4 shadowViewProj;	// シャドウマップ用ビュー・プロジェクション行列
};

/**
 * @brief CPU用スポットライト構造体
 * 
 * GPU側のデータに加え、アニメーション処理とシャドウマップ用のメンバを持つ
 * CPU側で管理するスポットライト。
 * シャドウマップを使用してリアルな影の描画が可能。
 */
struct CPUSpotLight {
    GPUSpotLight gpuData;   // GPU送信用データ

    // 色補間用メンバ
    Vector4 startColor;      // グラデーション開始色
    Vector4 endColor;        // グラデーション終了色
    float duration;          // 補間にかける時間（秒）
    float elapsedTime;       // 経過時間（秒）
    bool isReversing;        // 補間の方向（往復用）
    bool isGradientActive;   // グラデーションが有効かどうか
    std::function<float(float)> easingFunction; // イージング関数

    // シャドウマップ用（CPU側での計算用、GPUには gpuData.shadowViewProj を使用）
    Matrix4x4 viewMatrix;           // ライトビュー行列
    Matrix4x4 projectionMatrix;     // ライトプロジェクション行列
    Matrix4x4 viewProjectionMatrix; // ビュー・プロジェクション行列
    bool shadowEnabled = false;     // シャドウ有効フラグ
};
} // namespace KCE
