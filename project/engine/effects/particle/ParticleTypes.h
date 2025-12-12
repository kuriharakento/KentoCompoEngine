#pragma once

#include "math/BlendMode.h"

/**
 * @brief シミュレーションモード
 */
enum class SimulationMode
{
	CPU, ///< CPU計算
	GPU  ///< GPU計算（コンピュートシェーダー）
};

/**
 * @brief シミュレーション空間
 */
enum class SimulationSpace
{
	World, ///< ワールド空間（エミッター移動に追従しない）
	Local  ///< ローカル空間（エミッター移動に追従）
};

/**
 * @brief プリミティブ形状タイプ
 */
enum class PrimitiveType
{
	Plane,     ///< 平面
	Ring,      ///< リング（ドーナツ形状）
	Cylinder,  ///< 円柱
	Sphere,    ///< 球体
	Torus,     ///< トーラス（チューブ付きドーナツ）
	Star,      ///< 星形
	Heart,     ///< ハート形
	Spiral,    ///< スパイラル（螺旋）
	Cone,      ///< 円錐
	Cube,      ///< 立方体
	Custom     ///< カスタムメッシュ
};

/**
 * @brief レンダラータイプ
 */
enum class RendererType
{
	Sprite, ///< スプライト（ビルボード）
	Ribbon, ///< リボン（軌跡）
	Mesh    ///< メッシュ（3Dプリミティブ）
};

/**
 * @brief リボンテクスチャモード
 */
enum class RibbonTextureMode
{
	Stretch,  ///< 全体に引き伸ばし
	Tile      ///< タイル状に繰り返し
};

/**
 * @brief テクスチャシート再生モード
 */
enum class TextureSheetPlayMode
{
	Loop,     ///< ループ再生
	Once,     ///< 1回のみ再生
	PingPong  ///< 往復再生
};

/**
 * @brief フォースフィールドの減衰タイプ
 */
enum class FalloffType
{
	None,           ///< 減衰なし（一定の力）
	Linear,         ///< 線形減衰（距離に比例）
	InverseSquare   ///< 逆二乗減衰（物理的な重力・電磁力）
};
