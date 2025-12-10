#pragma once

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

#include "math/BlendMode.h"

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
 * @brief モジュール実行フェーズ
 */
enum class ModulePhase
{
	EmitterSpawn,
	EmitterUpdate,
	ParticleSpawn,
	ParticleUpdate
};
