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

	// 表面
	mesh.vertices = {
		{{ -0.5f, 0.0f, -0.5f }, { 0, 1, 0 }, { 0, 1 }},
		{{  0.5f, 0.0f, -0.5f }, { 0, 1, 0 }, { 1, 1 }},
		{{  0.5f, 0.0f,  0.5f }, { 0, 1, 0 }, { 1, 0 }},
		{{ -0.5f, 0.0f,  0.5f }, { 0, 1, 0 }, { 0, 0 }},
	};

	mesh.indices = { 0, 1, 2, 0, 2, 3 };

	if (doubleSided)
	{
		// 裏面
		mesh.vertices.push_back({{ -0.5f, 0.0f, -0.5f }, { 0, -1, 0 }, { 0, 1 }});
		mesh.vertices.push_back({{  0.5f, 0.0f,  0.5f }, { 0, -1, 0 }, { 1, 0 }});
		mesh.vertices.push_back({{  0.5f, 0.0f, -0.5f }, { 0, -1, 0 }, { 1, 1 }});
		mesh.vertices.push_back({{ -0.5f, 0.0f,  0.5f }, { 0, -1, 0 }, { 0, 0 }});

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

	for (uint32_t y = 0; y <= rings; ++y)
	{
		float v = static_cast<float>(y) / rings;
		float phi = v * kPi;

		for (uint32_t x = 0; x <= segments; ++x)
		{
			float u = static_cast<float>(x) / segments;
			float theta = u * kTwoPi;

			PrimitiveVertex vertex;
			vertex.position.x = std::sin(phi) * std::cos(theta) * 0.5f;
			vertex.position.y = std::cos(phi) * 0.5f;
			vertex.position.z = std::sin(phi) * std::sin(theta) * 0.5f;

			vertex.normal.x = std::sin(phi) * std::cos(theta);
			vertex.normal.y = std::cos(phi);
			vertex.normal.z = std::sin(phi) * std::sin(theta);

			vertex.texcoord = { u, v };

			mesh.vertices.push_back(vertex);
		}
	}

	for (uint32_t y = 0; y < rings; ++y)
	{
		for (uint32_t x = 0; x < segments; ++x)
		{
			uint32_t current = y * (segments + 1) + x;
			uint32_t next = current + segments + 1;

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

PrimitiveMesh PrimitiveGenerator::GenerateCylinder(uint32_t segments, bool withCaps)
{
	PrimitiveMesh mesh;
	float halfHeight = 0.5f;
	float radius = 0.5f;

	// 側面
	for (uint32_t i = 0; i <= segments; ++i)
	{
		float u = static_cast<float>(i) / segments;
		float theta = u * kTwoPi;
		float x = std::cos(theta) * radius;
		float z = std::sin(theta) * radius;

		// 下端
		mesh.vertices.push_back({{ x, -halfHeight, z }, { std::cos(theta), 0, std::sin(theta) }, { u, 1 }});
		// 上端
		mesh.vertices.push_back({{ x,  halfHeight, z }, { std::cos(theta), 0, std::sin(theta) }, { u, 0 }});
	}

	// 側面インデックス
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

	// 蓋
	if (withCaps)
	{
		// 上面の中心
		uint32_t topCenterIndex = static_cast<uint32_t>(mesh.vertices.size());
		mesh.vertices.push_back({{ 0,  halfHeight, 0 }, { 0, 1, 0 }, { 0.5f, 0.5f }});

		// 下面の中心
		uint32_t bottomCenterIndex = static_cast<uint32_t>(mesh.vertices.size());
		mesh.vertices.push_back({{ 0, -halfHeight, 0 }, { 0, -1, 0 }, { 0.5f, 0.5f }});

		// 蓋の頂点
		uint32_t capStartIndex = static_cast<uint32_t>(mesh.vertices.size());
		for (uint32_t i = 0; i <= segments; ++i)
		{
			float u = static_cast<float>(i) / segments;
			float theta = u * kTwoPi;
			float x = std::cos(theta) * radius;
			float z = std::sin(theta) * radius;

			// 上蓋
			mesh.vertices.push_back({{ x,  halfHeight, z }, { 0, 1, 0 }, { x + 0.5f, z + 0.5f }});
			// 下蓋
			mesh.vertices.push_back({{ x, -halfHeight, z }, { 0, -1, 0 }, { x + 0.5f, z + 0.5f }});
		}

		// 蓋インデックス
		for (uint32_t i = 0; i < segments; ++i)
		{
			// 上蓋
			mesh.indices.push_back(topCenterIndex);
			mesh.indices.push_back(capStartIndex + i * 2);
			mesh.indices.push_back(capStartIndex + (i + 1) * 2);

			// 下蓋
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

	// 頂点
	uint32_t tipIndex = 0;
	mesh.vertices.push_back({{ 0, height * 0.5f, 0 }, { 0, 1, 0 }, { 0.5f, 0 }});

	// 底面の頂点
	for (uint32_t i = 0; i <= segments; ++i)
	{
		float u = static_cast<float>(i) / segments;
		float theta = u * kTwoPi;
		float x = std::cos(theta) * radius;
		float z = std::sin(theta) * radius;

		float ny = radius / height;
		float nxz = 1.0f / std::sqrt(1 + ny * ny);
		ny *= nxz;

		mesh.vertices.push_back({{ x, -height * 0.5f, z }, { std::cos(theta) * nxz, ny, std::sin(theta) * nxz }, { u, 1 }});
	}

	// 側面インデックス
	for (uint32_t i = 0; i < segments; ++i)
	{
		mesh.indices.push_back(tipIndex);
		mesh.indices.push_back(1 + i + 1);
		mesh.indices.push_back(1 + i);
	}

	// 底面
	if (withCap)
	{
		uint32_t bottomCenterIndex = static_cast<uint32_t>(mesh.vertices.size());
		mesh.vertices.push_back({{ 0, -height * 0.5f, 0 }, { 0, -1, 0 }, { 0.5f, 0.5f }});

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

		// 外側
		mesh.vertices.push_back({{ cosT * outerRadius, 0, sinT * outerRadius }, { 0, 1, 0 }, { u, 0 }});
		// 内側
		mesh.vertices.push_back({{ cosT * innerRadius, 0, sinT * innerRadius }, { 0, 1, 0 }, { u, 1 }});
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

			mesh.vertices.push_back({{ x, y, z }, { nx, ny, nz }, { u, v }});
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

	// 各面（6面 × 4頂点）
	// 前面
	mesh.vertices.push_back({{ -s, -s,  s }, { 0, 0, 1 }, { 0, 1 }});
	mesh.vertices.push_back({{  s, -s,  s }, { 0, 0, 1 }, { 1, 1 }});
	mesh.vertices.push_back({{  s,  s,  s }, { 0, 0, 1 }, { 1, 0 }});
	mesh.vertices.push_back({{ -s,  s,  s }, { 0, 0, 1 }, { 0, 0 }});

	// 後面
	mesh.vertices.push_back({{  s, -s, -s }, { 0, 0, -1 }, { 0, 1 }});
	mesh.vertices.push_back({{ -s, -s, -s }, { 0, 0, -1 }, { 1, 1 }});
	mesh.vertices.push_back({{ -s,  s, -s }, { 0, 0, -1 }, { 1, 0 }});
	mesh.vertices.push_back({{  s,  s, -s }, { 0, 0, -1 }, { 0, 0 }});

	// 上面
	mesh.vertices.push_back({{ -s,  s,  s }, { 0, 1, 0 }, { 0, 1 }});
	mesh.vertices.push_back({{  s,  s,  s }, { 0, 1, 0 }, { 1, 1 }});
	mesh.vertices.push_back({{  s,  s, -s }, { 0, 1, 0 }, { 1, 0 }});
	mesh.vertices.push_back({{ -s,  s, -s }, { 0, 1, 0 }, { 0, 0 }});

	// 下面
	mesh.vertices.push_back({{ -s, -s, -s }, { 0, -1, 0 }, { 0, 1 }});
	mesh.vertices.push_back({{  s, -s, -s }, { 0, -1, 0 }, { 1, 1 }});
	mesh.vertices.push_back({{  s, -s,  s }, { 0, -1, 0 }, { 1, 0 }});
	mesh.vertices.push_back({{ -s, -s,  s }, { 0, -1, 0 }, { 0, 0 }});

	// 右面
	mesh.vertices.push_back({{  s, -s,  s }, { 1, 0, 0 }, { 0, 1 }});
	mesh.vertices.push_back({{  s, -s, -s }, { 1, 0, 0 }, { 1, 1 }});
	mesh.vertices.push_back({{  s,  s, -s }, { 1, 0, 0 }, { 1, 0 }});
	mesh.vertices.push_back({{  s,  s,  s }, { 1, 0, 0 }, { 0, 0 }});

	// 左面
	mesh.vertices.push_back({{ -s, -s, -s }, { -1, 0, 0 }, { 0, 1 }});
	mesh.vertices.push_back({{ -s, -s,  s }, { -1, 0, 0 }, { 1, 1 }});
	mesh.vertices.push_back({{ -s,  s,  s }, { -1, 0, 0 }, { 1, 0 }});
	mesh.vertices.push_back({{ -s,  s, -s }, { -1, 0, 0 }, { 0, 0 }});

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

	// 中心
	mesh.vertices.push_back({{ 0, 0, 0 }, { 0, 0, 1 }, { 0.5f, 0.5f }});

	// 星の頂点
	for (uint32_t i = 0; i <= points * 2; ++i)
	{
		float angle = static_cast<float>(i) / (points * 2) * kTwoPi - kPi * 0.5f;
		float r = (i % 2 == 0) ? outerRadius : innerRadius;
		float x = std::cos(angle) * r;
		float y = std::sin(angle) * r;

		mesh.vertices.push_back({{ x, y, 0 }, { 0, 0, 1 }, { x + 0.5f, 0.5f - y }});
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

	// 中心
	mesh.vertices.push_back({{ 0, 0, 0 }, { 0, 0, 1 }, { 0.5f, 0.5f }});

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

		mesh.vertices.push_back({{ x, y, 0 }, { 0, 0, 1 }, { x + 0.5f, 0.5f - y }});
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

		mesh.vertices.push_back({{ x, y, 0 }, { 0, 0, 1 }, { t, 0 }});
		mesh.vertices.push_back({{ ox, oy, 0 }, { 0, 0, 1 }, { t, 1 }});
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
