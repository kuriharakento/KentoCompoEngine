#include "PrimitiveGenerator.h"
#include <cmath>

PrimitiveMesh PrimitiveGenerator::Generate(PrimitiveType type, const PrimitiveOptions& options)
{
	switch (type)
	{
	case PrimitiveType::Plane:
		return GeneratePlane(options.doubleSided);
	case PrimitiveType::Sphere:
		return GenerateSphere(options.segments, options.rings);
	case PrimitiveType::Cylinder:
		return GenerateCylinder(options.segments, options.withCaps);
	case PrimitiveType::Cone:
		return GenerateCone(options.segments, options.withCaps);
	case PrimitiveType::Ring:
		return GenerateRing(options.segments, options.innerRadius, options.outerRadius);
	case PrimitiveType::Torus:
		return GenerateTorus(options.segments, options.segments / 2, options.tubeRadius);
	case PrimitiveType::Cube:
		return GenerateCube();
	case PrimitiveType::Star:
		return GenerateStar(options.points, options.innerRadius);
	case PrimitiveType::Heart:
		return GenerateHeart(options.segments);
	case PrimitiveType::Spiral:
		return GenerateSpiral(options.segments, options.turns);
	default:
		return GeneratePlane();
	}
}

PrimitiveMesh PrimitiveGenerator::GeneratePlane(bool doubleSided)
{
	PrimitiveMesh mesh;

	// X-Y平面（正面向き、Z+方向を向く）(順序: position, texcoord, normal)
	mesh.vertices = {
		{{ -0.5f, -0.5f, 0.0f, 1.0f }, { 0, 1 }, { 0, 0, 1 }},
		{{  0.5f, -0.5f, 0.0f, 1.0f }, { 1, 1 }, { 0, 0, 1 }},
		{{  0.5f,  0.5f, 0.0f, 1.0f }, { 1, 0 }, { 0, 0, 1 }},
		{{ -0.5f,  0.5f, 0.0f, 1.0f }, { 0, 0 }, { 0, 0, 1 }},
	};

	mesh.indices = { 0, 1, 2, 0, 2, 3 };

	if (doubleSided)
	{
		// 裏面（Z-方向を向く）
		mesh.vertices.push_back({{ -0.5f, -0.5f, 0.0f, 1.0f }, { 0, 1 }, { 0, 0, -1 }});
		mesh.vertices.push_back({{  0.5f,  0.5f, 0.0f, 1.0f }, { 1, 0 }, { 0, 0, -1 }});
		mesh.vertices.push_back({{  0.5f, -0.5f, 0.0f, 1.0f }, { 1, 1 }, { 0, 0, -1 }});
		mesh.vertices.push_back({{ -0.5f,  0.5f, 0.0f, 1.0f }, { 0, 0 }, { 0, 0, -1 }});

		mesh.indices.push_back(4);
		mesh.indices.push_back(5);
		mesh.indices.push_back(6);
		mesh.indices.push_back(4);
		mesh.indices.push_back(7);
		mesh.indices.push_back(5);
	}

	return mesh;
}

PrimitiveMesh PrimitiveGenerator::GenerateSphere(uint32_t segments, uint32_t rings)
{
	PrimitiveMesh mesh;

	// 頂点生成: 球面座標系で位置・法線・UVを計算
	for (uint32_t y = 0; y <= rings; ++y)
	{
		float v = static_cast<float>(y) / rings;
		float phi = v * kPi;  // 垂直角（0～π）

		for (uint32_t x = 0; x <= segments; ++x)
		{
			float u = static_cast<float>(x) / segments;
			float theta = u * kTwoPi;  // 水平角（0～2π）

		PrimitiveVertex vertex;
			// 球面座標→直交座標変換（半径0.5）
			vertex.position.x = std::sin(phi) * std::cos(theta) * 0.5f;
			vertex.position.y = std::cos(phi) * 0.5f;
			vertex.position.z = std::sin(phi) * std::sin(theta) * 0.5f;
			vertex.position.w = 1.0f;

			vertex.texcoord = { u, v };

			// 法線は球の中心から外向き（正規化済み）
			vertex.normal.x = std::sin(phi) * std::cos(theta);
			vertex.normal.y = std::cos(phi);
			vertex.normal.z = std::sin(phi) * std::sin(theta);

			mesh.vertices.push_back(vertex);
		}
	}

	// インデックス生成: 四角形を2つの三角形に分割
	for (uint32_t y = 0; y < rings; ++y)
	{
		for (uint32_t x = 0; x < segments; ++x)
		{
			uint32_t current = y * (segments + 1) + x;
			uint32_t next = current + segments + 1;

			// 下三角形
			mesh.indices.push_back(current);
			mesh.indices.push_back(next);
			mesh.indices.push_back(current + 1);

			// 上三角形
			mesh.indices.push_back(current + 1);
			mesh.indices.push_back(next);
			mesh.indices.push_back(next + 1);
		}
	}

	return mesh;
}

PrimitiveMesh PrimitiveGenerator::GenerateCylinder(uint32_t segments, bool withCaps)
{
	PrimitiveMesh mesh;
	float halfHeight = 0.5f;
	float radius = 0.5f;

	// 側面の頂点生成: 各セグメントで上下2つの頂点を生成
	for (uint32_t i = 0; i <= segments; ++i)
	{
		float u = static_cast<float>(i) / segments;
		float theta = u * kTwoPi;
		float x = std::cos(theta) * radius;
		float z = std::sin(theta) * radius;

		// 下端の頂点
		mesh.vertices.push_back({{ x, -halfHeight, z, 1.0f }, { u, 1 }, { std::cos(theta), 0, std::sin(theta) }});
		// 上端の頂点
		mesh.vertices.push_back({{ x,  halfHeight, z, 1.0f }, { u, 0 }, { std::cos(theta), 0, std::sin(theta) }});
	}

	// 側面のインデックス生成: 四角形を2つの三角形に分割
	for (uint32_t i = 0; i < segments; ++i)
	{
		uint32_t base = i * 2;
		mesh.indices.push_back(base);
		mesh.indices.push_back(base + 1);
		mesh.indices.push_back(base + 2);

		mesh.indices.push_back(base + 1);
		mesh.indices.push_back(base + 3);
		mesh.indices.push_back(base + 2);
	}

	// 上下の蓋を生成
	if (withCaps)
	{
		// 上面の中心頂点
		uint32_t topCenterIndex = static_cast<uint32_t>(mesh.vertices.size());
		mesh.vertices.push_back({{ 0,  halfHeight, 0, 1.0f }, { 0.5f, 0.5f }, { 0, 1, 0 }});

		// 下面の中心頂点
		uint32_t bottomCenterIndex = static_cast<uint32_t>(mesh.vertices.size());
		mesh.vertices.push_back({{ 0, -halfHeight, 0, 1.0f }, { 0.5f, 0.5f }, { 0, -1, 0 }});

		// 蓋用の頂点（法線が上下を向く）
		uint32_t capStartIndex = static_cast<uint32_t>(mesh.vertices.size());
		for (uint32_t i = 0; i <= segments; ++i)
		{
			float u = static_cast<float>(i) / segments;
			float theta = u * kTwoPi;
			float x = std::cos(theta) * radius;
			float z = std::sin(theta) * radius;

			// 上蓋の頂点
			mesh.vertices.push_back({{ x,  halfHeight, z, 1.0f }, { x + 0.5f, z + 0.5f }, { 0, 1, 0 }});
			// 下蓋の頂点
			mesh.vertices.push_back({{ x, -halfHeight, z, 1.0f }, { x + 0.5f, z + 0.5f }, { 0, -1, 0 }});
		}

		// 蓋のインデックス生成: 扇形に三角形を配置
		for (uint32_t i = 0; i < segments; ++i)
		{
			// 上蓋の三角形
			mesh.indices.push_back(topCenterIndex);
			mesh.indices.push_back(capStartIndex + i * 2);
			mesh.indices.push_back(capStartIndex + (i + 1) * 2);

			// 下蓋の三角形（巻き順反転）
			mesh.indices.push_back(bottomCenterIndex);
			mesh.indices.push_back(capStartIndex + (i + 1) * 2 + 1);
			mesh.indices.push_back(capStartIndex + i * 2 + 1);
		}
	}

	return mesh;
}

PrimitiveMesh PrimitiveGenerator::GenerateCone(uint32_t segments, bool withCap)
{
	PrimitiveMesh mesh;
	float height = 1.0f;
	float radius = 0.5f;

	// 頂点（円錐の先端）
	uint32_t tipIndex = 0;
	mesh.vertices.push_back({{ 0, height * 0.5f, 0, 1.0f }, { 0.5f, 0 }, { 0, 1, 0 }});

	// 底面の円周上の頂点を生成
	for (uint32_t i = 0; i <= segments; ++i)
	{
		float u = static_cast<float>(i) / segments;
		float theta = u * kTwoPi;
		float x = std::cos(theta) * radius;
		float z = std::sin(theta) * radius;

		// 円錐の側面法線を計算（傾斜を考慮）
		float ny = radius / height;
		float nxz = 1.0f / std::sqrt(1 + ny * ny);
		ny *= nxz;

		mesh.vertices.push_back({{ x, -height * 0.5f, z, 1.0f }, { u, 1 }, { std::cos(theta) * nxz, ny, std::sin(theta) * nxz }});
	}

	// 側面のインデックス生成: 頂点から各底面頂点へ三角形を生成
	for (uint32_t i = 0; i < segments; ++i)
	{
		mesh.indices.push_back(tipIndex);
		mesh.indices.push_back(1 + i + 1);
		mesh.indices.push_back(1 + i);
	}

	// 底面の蓋を生成
	if (withCap)
	{
		// 底面の中心頂点
		uint32_t bottomCenterIndex = static_cast<uint32_t>(mesh.vertices.size());
		mesh.vertices.push_back({{ 0, -height * 0.5f, 0, 1.0f }, { 0.5f, 0.5f }, { 0, -1, 0 }});

		// 底面の三角形インデックス（扇形配置）
		for (uint32_t i = 0; i < segments; ++i)
		{
			mesh.indices.push_back(bottomCenterIndex);
			mesh.indices.push_back(1 + i);
			mesh.indices.push_back(1 + i + 1);
		}
	}

	return mesh;
}

PrimitiveMesh PrimitiveGenerator::GenerateRing(uint32_t segments, float innerRadius, float outerRadius)
{
	PrimitiveMesh mesh;

	for (uint32_t i = 0; i <= segments; ++i)
	{
		float u = static_cast<float>(i) / segments;
		float theta = u * kTwoPi;
		float cosT = std::cos(theta);
		float sinT = std::sin(theta);

		// 外側 (position, texcoord, normal)
		mesh.vertices.push_back({{ cosT * outerRadius, 0, sinT * outerRadius, 1.0f }, { u, 0 }, { 0, 1, 0 }});
		// 内側
		mesh.vertices.push_back({{ cosT * innerRadius, 0, sinT * innerRadius, 1.0f }, { u, 1 }, { 0, 1, 0 }});
	}

	for (uint32_t i = 0; i < segments; ++i)
	{
		uint32_t base = i * 2;
		mesh.indices.push_back(base);
		mesh.indices.push_back(base + 2);
		mesh.indices.push_back(base + 1);

		mesh.indices.push_back(base + 1);
		mesh.indices.push_back(base + 2);
		mesh.indices.push_back(base + 3);
	}

	return mesh;
}

PrimitiveMesh PrimitiveGenerator::GenerateTorus(uint32_t segments, uint32_t tubeSegments, float tubeRadius)
{
	PrimitiveMesh mesh;
	float majorRadius = 0.5f - tubeRadius;

	for (uint32_t i = 0; i <= segments; ++i)
	{
		float u = static_cast<float>(i) / segments;
		float theta = u * kTwoPi;
		float cosT = std::cos(theta);
		float sinT = std::sin(theta);

		for (uint32_t j = 0; j <= tubeSegments; ++j)
		{
			float v = static_cast<float>(j) / tubeSegments;
			float phi = v * kTwoPi;
			float cosPhi = std::cos(phi);
			float sinPhi = std::sin(phi);

			float x = (majorRadius + tubeRadius * cosPhi) * cosT;
			float y = tubeRadius * sinPhi;
			float z = (majorRadius + tubeRadius * cosPhi) * sinT;

			float nx = cosPhi * cosT;
			float ny = sinPhi;
			float nz = cosPhi * sinT;

			mesh.vertices.push_back({{ x, y, z, 1.0f }, { u, v }, { nx, ny, nz }});
		}
	}

	for (uint32_t i = 0; i < segments; ++i)
	{
		for (uint32_t j = 0; j < tubeSegments; ++j)
		{
			uint32_t current = i * (tubeSegments + 1) + j;
			uint32_t next = current + tubeSegments + 1;

			mesh.indices.push_back(current);
			mesh.indices.push_back(next);
			mesh.indices.push_back(current + 1);

			mesh.indices.push_back(current + 1);
			mesh.indices.push_back(next);
			mesh.indices.push_back(next + 1);
		}
	}

	return mesh;
}

PrimitiveMesh PrimitiveGenerator::GenerateCube()
{
	PrimitiveMesh mesh;
	float s = 0.5f;

	// 各面（6面 × 4頂点）(position, texcoord, normal)
	// 前面
	mesh.vertices.push_back({{ -s, -s,  s, 1.0f }, { 0, 1 }, { 0, 0, 1 }});
	mesh.vertices.push_back({{  s, -s,  s, 1.0f }, { 1, 1 }, { 0, 0, 1 }});
	mesh.vertices.push_back({{  s,  s,  s, 1.0f }, { 1, 0 }, { 0, 0, 1 }});
	mesh.vertices.push_back({{ -s,  s,  s, 1.0f }, { 0, 0 }, { 0, 0, 1 }});

	// 後面
	mesh.vertices.push_back({{  s, -s, -s, 1.0f }, { 0, 1 }, { 0, 0, -1 }});
	mesh.vertices.push_back({{ -s, -s, -s, 1.0f }, { 1, 1 }, { 0, 0, -1 }});
	mesh.vertices.push_back({{ -s,  s, -s, 1.0f }, { 1, 0 }, { 0, 0, -1 }});
	mesh.vertices.push_back({{  s,  s, -s, 1.0f }, { 0, 0 }, { 0, 0, -1 }});

	// 上面
	mesh.vertices.push_back({{ -s,  s,  s, 1.0f }, { 0, 1 }, { 0, 1, 0 }});
	mesh.vertices.push_back({{  s,  s,  s, 1.0f }, { 1, 1 }, { 0, 1, 0 }});
	mesh.vertices.push_back({{  s,  s, -s, 1.0f }, { 1, 0 }, { 0, 1, 0 }});
	mesh.vertices.push_back({{ -s,  s, -s, 1.0f }, { 0, 0 }, { 0, 1, 0 }});

	// 下面
	mesh.vertices.push_back({{ -s, -s, -s, 1.0f }, { 0, 1 }, { 0, -1, 0 }});
	mesh.vertices.push_back({{  s, -s, -s, 1.0f }, { 1, 1 }, { 0, -1, 0 }});
	mesh.vertices.push_back({{  s, -s,  s, 1.0f }, { 1, 0 }, { 0, -1, 0 }});
	mesh.vertices.push_back({{ -s, -s,  s, 1.0f }, { 0, 0 }, { 0, -1, 0 }});

	// 右面
	mesh.vertices.push_back({{  s, -s,  s, 1.0f }, { 0, 1 }, { 1, 0, 0 }});
	mesh.vertices.push_back({{  s, -s, -s, 1.0f }, { 1, 1 }, { 1, 0, 0 }});
	mesh.vertices.push_back({{  s,  s, -s, 1.0f }, { 1, 0 }, { 1, 0, 0 }});
	mesh.vertices.push_back({{  s,  s,  s, 1.0f }, { 0, 0 }, { 1, 0, 0 }});

	// 左面
	mesh.vertices.push_back({{ -s, -s, -s, 1.0f }, { 0, 1 }, { -1, 0, 0 }});
	mesh.vertices.push_back({{ -s, -s,  s, 1.0f }, { 1, 1 }, { -1, 0, 0 }});
	mesh.vertices.push_back({{ -s,  s,  s, 1.0f }, { 1, 0 }, { -1, 0, 0 }});
	mesh.vertices.push_back({{ -s,  s, -s, 1.0f }, { 0, 0 }, { -1, 0, 0 }});

	// インデックス
	for (uint32_t face = 0; face < 6; ++face)
	{
		uint32_t base = face * 4;
		mesh.indices.push_back(base + 0);
		mesh.indices.push_back(base + 1);
		mesh.indices.push_back(base + 2);
		mesh.indices.push_back(base + 0);
		mesh.indices.push_back(base + 2);
		mesh.indices.push_back(base + 3);
	}

	return mesh;
}

PrimitiveMesh PrimitiveGenerator::GenerateStar(uint32_t points, float innerRadius)
{
	PrimitiveMesh mesh;
	float outerRadius = 0.5f;

	// 中心 (position, texcoord, normal)
	mesh.vertices.push_back({{ 0, 0, 0, 1.0f }, { 0.5f, 0.5f }, { 0, 0, 1 }});

	// 星の頂点
	for (uint32_t i = 0; i <= points * 2; ++i)
	{
		float angle = static_cast<float>(i) / (points * 2) * kTwoPi - kPi * 0.5f;
		float r = (i % 2 == 0) ? outerRadius : innerRadius;
		float x = std::cos(angle) * r;
		float y = std::sin(angle) * r;

		mesh.vertices.push_back({{ x, y, 0, 1.0f }, { x + 0.5f, 0.5f - y }, { 0, 0, 1 }});
	}

	// インデックス
	for (uint32_t i = 0; i < points * 2; ++i)
	{
		mesh.indices.push_back(0);
		mesh.indices.push_back(1 + i);
		mesh.indices.push_back(1 + (i + 1) % (points * 2));
	}

	return mesh;
}

PrimitiveMesh PrimitiveGenerator::GenerateHeart(uint32_t segments)
{
	PrimitiveMesh mesh;

	// 中心 (position, texcoord, normal)
	mesh.vertices.push_back({{ 0, 0, 0, 1.0f }, { 0.5f, 0.5f }, { 0, 0, 1 }});

	// ハート形状
	for (uint32_t i = 0; i <= segments; ++i)
	{
		float t = static_cast<float>(i) / segments * kTwoPi;
		
		// ハートの数式
		float x = 16.0f * std::sin(t) * std::sin(t) * std::sin(t);
		float y = 13.0f * std::cos(t) - 5.0f * std::cos(2 * t) - 2.0f * std::cos(3 * t) - std::cos(4 * t);

		// スケール調整
		x *= 0.03f;
		y *= 0.03f;

		mesh.vertices.push_back({{ x, y, 0, 1.0f }, { x + 0.5f, 0.5f - y }, { 0, 0, 1 }});
	}

	// インデックス
	for (uint32_t i = 0; i < segments; ++i)
	{
		mesh.indices.push_back(0);
		mesh.indices.push_back(1 + i);
		mesh.indices.push_back(1 + i + 1);
	}

	return mesh;
}

PrimitiveMesh PrimitiveGenerator::GenerateSpiral(uint32_t segments, float turns)
{
	PrimitiveMesh mesh;
	float width = 0.1f;

	for (uint32_t i = 0; i <= segments; ++i)
	{
		float t = static_cast<float>(i) / segments;
		float angle = t * kTwoPi * turns;
		float radius = t * 0.5f;

		float x = std::cos(angle) * radius;
		float y = std::sin(angle) * radius;

		// 外側
		float ox = std::cos(angle) * (radius + width);
		float oy = std::sin(angle) * (radius + width);

		mesh.vertices.push_back({{ x, y, 0, 1.0f }, { t, 0 }, { 0, 0, 1 }});
		mesh.vertices.push_back({{ ox, oy, 0, 1.0f }, { t, 1 }, { 0, 0, 1 }});
	}

	for (uint32_t i = 0; i < segments; ++i)
	{
		uint32_t base = i * 2;
		mesh.indices.push_back(base);
		mesh.indices.push_back(base + 2);
		mesh.indices.push_back(base + 1);

		mesh.indices.push_back(base + 1);
		mesh.indices.push_back(base + 2);
		mesh.indices.push_back(base + 3);
	}

	return mesh;
}
