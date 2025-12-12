#pragma once
/**
 * @file PrimitiveGenerator.h
 * @brief プリミティブ形状生成ユーティリティ
 * 
 * Plane, Sphere, Cube, Cone, Torus, Star, Heart等の
 * 各種プリミティブメッシュを動的に生成。
 */
#include "math/Vector3.h"
#include "math/Vector2.h"
#include "math/Vector4.h"
#include "effects/particle/ParticleTypes.h"
#include <vector>
#include <cmath>

/**
 * @brief プリミティブ頂点データ
 * @note Particle.VS.hlslのVertexShaderInputと一致するレイアウト
 */
struct PrimitiveVertex
{
	Vector4 position;  // POSITION0 (float4)
	Vector2 texcoord;  // TEXCOORD0 (float2)
	Vector3 normal;    // NORMAL0 (float3)
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
	uint32_t segments = 16;         // 分割数
	uint32_t rings = 8;             // 球体のリング数
	float innerRadius = 0.5f;       // Ring/Starの内側半径
	float outerRadius = 1.0f;       // Ring/Starの外側半径
	float tubeRadius = 0.3f;        // Torusのチューブ半径
	float turns = 2.0f;             // Spiralの回転数
	uint32_t points = 5;            // Starの頂点数
	bool withCaps = true;           // Cylinder/Coneの蓋あり
	bool doubleSided = false;       // 両面描画用（裏面も生成）
};

/**
 * @brief プリミティブ形状生成ユーティリティ
 */
class PrimitiveGenerator
{
public:
	/**
	 * @brief プリミティブタイプから頂点データを生成
	 * @param type プリミティブタイプ
	 * @param options 生成オプション
	 * @return 生成されたメッシュデータ
	 */
	static PrimitiveMesh Generate(PrimitiveType type, const PrimitiveOptions& options = {});

	/**
	 * @brief 平面（Plane）を生成
	 * @param doubleSided 両面描画用に裏面も生成するか
	 * @return 生成されたメッシュデータ
	 */
	static PrimitiveMesh GeneratePlane(bool doubleSided = false);

	/**
	 * @brief 球体（Sphere）を生成
	 * @param segments 水平方向の分割数（デフォルト: 16）
	 * @param rings 垂直方向のリング数（デフォルト: 8）
	 * @return 生成されたメッシュデータ
	 */
	static PrimitiveMesh GenerateSphere(uint32_t segments = 16, uint32_t rings = 8);

	/**
	 * @brief 円柱（Cylinder）を生成
	 * @param segments 円周方向の分割数（デフォルト: 16）
	 * @param withCaps 上下の蓋をつけるか
	 * @return 生成されたメッシュデータ
	 */
	static PrimitiveMesh GenerateCylinder(uint32_t segments = 16, bool withCaps = true);

	/**
	 * @brief 円錐（Cone）を生成
	 * @param segments 円周方向の分割数（デフォルト: 16）
	 * @param withCap 底面の蓋をつけるか
	 * @return 生成されたメッシュデータ
	 */
	static PrimitiveMesh GenerateCone(uint32_t segments = 16, bool withCap = true);

	/**
	 * @brief リング（Ring）を生成
	 * @param segments 円周方向の分割数（デフォルト: 32）
	 * @param innerRadius 内側の半径（デフォルト: 0.5）
	 * @param outerRadius 外側の半径（デフォルト: 1.0）
	 * @return 生成されたメッシュデータ
	 */
	static PrimitiveMesh GenerateRing(uint32_t segments = 32, float innerRadius = 0.5f, float outerRadius = 1.0f);

	/**
	 * @brief トーラス（Torus）を生成
	 * @param segments 大円の分割数（デフォルト: 16）
	 * @param tubeSegments チューブの分割数（デフォルト: 8）
	 * @param tubeRadius チューブの半径（デフォルト: 0.3）
	 * @return 生成されたメッシュデータ
	 */
	static PrimitiveMesh GenerateTorus(uint32_t segments = 16, uint32_t tubeSegments = 8, float tubeRadius = 0.3f);

	/**
	 * @brief 立方体（Cube）を生成
	 * @return 生成されたメッシュデータ
	 */
	static PrimitiveMesh GenerateCube();

	/**
	 * @brief 星形（Star）を生成
	 * @param points 星の頂点数（デフォルト: 5）
	 * @param innerRadius 内側の半径（デフォルト: 0.4）
	 * @return 生成されたメッシュデータ
	 */
	static PrimitiveMesh GenerateStar(uint32_t points = 5, float innerRadius = 0.4f);

	/**
	 * @brief ハート形（Heart）を生成
	 * @param segments 曲線の分割数（デフォルト: 32）
	 * @return 生成されたメッシュデータ
	 */
	static PrimitiveMesh GenerateHeart(uint32_t segments = 32);

	/**
	 * @brief スパイラル（Spiral）を生成
	 * @param segments 螺旋の分割数（デフォルト: 64）
	 * @param turns 回転数（デフォルト: 2.0）
	 * @return 生成されたメッシュデータ
	 */
	static PrimitiveMesh GenerateSpiral(uint32_t segments = 64, float turns = 2.0f);

private:
	static constexpr float kPi = 3.14159265358979f;
	static constexpr float kTwoPi = kPi * 2.0f;
};
