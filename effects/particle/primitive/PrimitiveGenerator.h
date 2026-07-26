#pragma once
/**
 * @file PrimitiveGenerator.h
 * @brief プリミティブ形状生成ユーティリティ
 * 
 * Plane, Sphere, Cube, Cone, Torus, Star, Heart等の
 * 各種プリミティブメッシュを動的に生成する。
 */
#include "math/Vector3.h"
#include "math/Vector2.h"
#include "math/Vector4.h"
#include "effects/particle/ParticleTypes.h"
#include <vector>
#include <cmath>

namespace KCE
{
/**
 * @brief プリミティブ頂点データ
 * @note Particle.VS.hlslのVertexShaderInputと一致するレイアウト
 */
struct PrimitiveVertex
{
	// POSITION0 (float4)
	Vector4 position;
	// TEXCOORD0 (float2)
	Vector2 texcoord;
	// NORMAL0 (float3)
	Vector3 normal;
};

/**
 * @brief プリミティブメッシュデータ
 */
struct PrimitiveMesh
{
	std::vector<PrimitiveVertex> vertices;
	std::vector<uint32_t> indices;
};

/**
 * @brief プリミティブ生成オプション
 */
struct PrimitiveOptions
{
	// 分割数
	uint32_t segments = 16;
	// 球体のリング数
	uint32_t rings = 8;
	// Ring/Starの内側半径
	float innerRadius = 0.5f;
	// Ring/Starの外側半径
	float outerRadius = 1.0f;
	// Torusのチューブ半径
	float tubeRadius = 0.3f;
	// Spiralの回転数
	float turns = 2.0f;
	// Starの頂点数
	uint32_t points = 5;
	// Cylinder/Coneの蓋あり
	bool withCaps = true;
	// 両面描画用（裏面も生成）
	bool doubleSided = false;
	// Cubeのサイズ (非一様スケール用)
	Vector3 cubeSize = { 1.0f, 1.0f, 1.0f };
	// Cubeの各面の表示フラグ (前, 後, 上, 下, 右, 左)
	bool cubeFaceVisible[6] = { true, true, true, true, true, true };
};

/**
 * @brief プリミティブ形状生成ユーティリティ
 */
class PrimitiveGenerator
{
public:
	/**
	 * @brief プリミティブタイプから頂点データを生成する。
	 * @param type プリミティブタイプ
	 * @param options 生成オプション
	 * @return 生成されたメッシュデータ
	 */
	static PrimitiveMesh Generate(PrimitiveType type, const PrimitiveOptions& options = {});

	/**
	 * @brief 平面（Plane）を生成する。
	 * @param doubleSided 両面描画用に裏面も生成するか
	 * @return 生成されたメッシュデータ
	 */
	static PrimitiveMesh GeneratePlane(bool doubleSided = false);

	/**
	 * @brief 球体（Sphere）を生成する。
	 * @param segments 水平方向の分割数（デフォルト: 16）
	 * @param rings 垂直方向のリング数（デフォルト: 8）
	 * @return 生成されたメッシュデータ
	 */
	static PrimitiveMesh GenerateSphere(uint32_t segments = 16, uint32_t rings = 8);

	/**
	 * @brief 円柱（Cylinder）を生成する。
	 * @param segments 円周方向の分割数（デフォルト: 16）
	 * @param withCaps 上下の蓋をつけるか
	 * @return 生成されたメッシュデータ
	 */
	static PrimitiveMesh GenerateCylinder(uint32_t segments = 16, bool withCaps = true);

	/**
	 * @brief 円錐（Cone）を生成する。
	 * @param segments 円周方向の分割数（デフォルト: 16）
	 * @param withCap 底面の蓋をつけるか
	 * @return 生成されたメッシュデータ
	 */
	static PrimitiveMesh GenerateCone(uint32_t segments = 16, bool withCap = true);

	/**
	 * @brief リング（Ring）を生成する。
	 * @param segments 円周方向の分割数（デフォルト: 32）
	 * @param innerRadius 内側の半径（デフォルト: 0.5）
	 * @param outerRadius 外側の半径（デフォルト: 1.0）
	 * @return 生成されたメッシュデータ
	 */
	static PrimitiveMesh GenerateRing(uint32_t segments = 32, float innerRadius = 0.5f, float outerRadius = 1.0f);

	/**
	 * @brief トーラス（Torus）を生成する。
	 * @param segments 大円の分割数（デフォルト: 16）
	 * @param tubeSegments チューブの分割数（デフォルト: 8）
	 * @param tubeRadius チューブの半径（デフォルト: 0.3）
	 * @return 生成されたメッシュデータ
	 */
	static PrimitiveMesh GenerateTorus(uint32_t segments = 16, uint32_t tubeSegments = 8, float tubeRadius = 0.3f);

	/**
	 * @brief 立方体（Cube）を生成する。
	 * @return 生成されたメッシュデータ
	 */
	/**
	 * @brief 立方体（Cube）を生成する。
	 * @param options 生成オプション（サイズ、面表示）
	 * @return 生成されたメッシュデータ
	 */
	static PrimitiveMesh GenerateCube(const PrimitiveOptions& options = {});

	/**
	 * @brief 星形（Star）を生成する。
	 * @param points 星の頂点数（デフォルト: 5）
	 * @param innerRadius 内側の半径（デフォルト: 0.4）
	 * @return 生成されたメッシュデータ
	 */
	static PrimitiveMesh GenerateStar(uint32_t points = 5, float innerRadius = 0.4f);

	/**
	 * @brief ハート形（Heart）を生成する。
	 * @param segments 曲線の分割数（デフォルト: 32）
	 * @return 生成されたメッシュデータ
	 */
	static PrimitiveMesh GenerateHeart(uint32_t segments = 32);

	/**
	 * @brief スパイラル（Spiral）を生成する。
	 * @param segments 螺旋の分割数（デフォルト: 64）
	 * @param turns 回転数（デフォルト: 2.0）
	 * @return 生成されたメッシュデータ
	 */
	static PrimitiveMesh GenerateSpiral(uint32_t segments = 64, float turns = 2.0f);

private:
	static constexpr float kPi = 3.14159265358979f;
	static constexpr float kTwoPi = kPi * 2.0f;
};
} // namespace KCE
