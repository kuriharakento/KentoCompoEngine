#pragma once
#include <cassert>
#include <d3d12.h>
#include <wrl.h>

#include "DirectXTex/d3dx12.h"
#include "math/MatrixFunc.h"

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
	Vector4 color;					// ライトの色（RGBAで指定）
	Vector3 position;				// ライトの位置（ワールド座標）
	float intensity;				// ライトの強さ（明るさの倍率）
	Vector3 direction;				// ライトの向き（正規化されたベクトル）
	float distance;					// ライトの届く最大距離
	float decay;					// ライトの減衰率（高いほど早く減衰）
	float cosAngle;					// ライト円錐角度の余弦（内側境界）
	float cosFalloffStart;			// フォールオフ開始角度の余弦（外側境界）
};