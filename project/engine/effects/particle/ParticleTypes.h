#pragma once

#include "math/BlendMode.h"

/**
 * @brief シミュレーションモード
 */
enum class SimulationMode
{
	CPU, // CPU計算
	GPU  // GPU計算
};

/**
 * @brief シミュレーション空間
 */
enum class SimulationSpace
{
	World, // ワールド空間
	Local  // ローカル空間
};

/**
 * @brief プリミティブ形状タイプ
 */
enum class PrimitiveType
{
	Plane,
	Ring,
	Cylinder,
	Sphere,
	Torus,
	Star,
	Heart,
	Spiral,
	Cone,
	Cube,
	Custom
};

/**
 * @brief レンダラータイプ
 */
enum class RendererType
{
	Sprite,
	Ribbon,
	Mesh
};

/**
 * @brief リボンテクスチャモード
 */
enum class RibbonTextureMode
{
	Stretch,  // 全体に引き伸ばし
	Tile      // タイル状に繰り返し
};

/**
 * @brief テクスチャシート再生モード
 */
enum class TextureSheetPlayMode
{
	Loop,     // ループ再生
	Once,     // 1回のみ
	PingPong  // 往復
};

/**
 * @brief フォースフィールドの減衰タイプ
 */
enum class FalloffType
{
	None,           // 減衰なし（一定）
	Linear,         // 線形減衰
	InverseSquare   // 逆二乗減衰
};
