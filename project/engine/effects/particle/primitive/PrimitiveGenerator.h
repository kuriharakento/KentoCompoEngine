#pragma once
#include "math/Vector3.h"
#include "math/Vector2.h"
#include "math/Vector4.h"
#include "effects/particle/ParticleTypes.h"
#include <vector>
#include <cmath>

/**
 * @brief プリミティブ頂点データ
 */
struct PrimitiveVertex
{
	Vector3 position;
	Vector3 normal;
	Vector2 texcoord;
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
	 */
	static PrimitiveMesh Generate(PrimitiveType type, const PrimitiveOptions& options = {});

	/**
	 * @brief 平面（Plane）を生成
	 */
	static PrimitiveMesh GeneratePlane(bool doubleSided = false);

	/**
	 * @brief 球体（Sphere）を生成
	 */
	static PrimitiveMesh GenerateSphere(uint32_t segments = 16, uint32_t rings = 8);

	/**
	 * @brief 円柱（Cylinder）を生成
	 * @param withCaps 蓋をつけるか
	 */
	static PrimitiveMesh GenerateCylinder(uint32_t segments = 16, bool withCaps = true);

	/**
	 * @brief 円錐（Cone）を生成
	 * @param withCap 底面の蓋をつけるか
	 */
	static PrimitiveMesh GenerateCone(uint32_t segments = 16, bool withCap = true);

	/**
	 * @brief リング（Ring）を生成
	 */
	static PrimitiveMesh GenerateRing(uint32_t segments = 32, float innerRadius = 0.5f, float outerRadius = 1.0f);

	/**
	 * @brief トーラス（Torus）を生成
	 */
	static PrimitiveMesh GenerateTorus(uint32_t segments = 16, uint32_t tubeSegments = 8, float tubeRadius = 0.3f);

	/**
	 * @brief 立方体（Cube）を生成
	 */
	static PrimitiveMesh GenerateCube();

	/**
	 * @brief 星形（Star）を生成
	 */
	static PrimitiveMesh GenerateStar(uint32_t points = 5, float innerRadius = 0.4f);

	/**
	 * @brief ハート形（Heart）を生成
	 */
	static PrimitiveMesh GenerateHeart(uint32_t segments = 32);

	/**
	 * @brief スパイラル（Spiral）を生成
	 */
	static PrimitiveMesh GenerateSpiral(uint32_t segments = 64, float turns = 2.0f);

private:
	static constexpr float kPi = 3.14159265358979f;
	static constexpr float kTwoPi = kPi * 2.0f;
};
