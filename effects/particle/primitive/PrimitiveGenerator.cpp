#include "PrimitiveGenerator.h"
#include <numbers>
#include <cmath>

namespace KCE
{
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
		return GenerateCube(options);
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

	// 平面生成 (Radius 1.0, Size 2.0)
	std::vector<PrimitiveVertex> rectangleVertices = {
		{ {  1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }, // 右上
		{ { -1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }, // 左上
		{ {  1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } }, // 右下
		{ {  1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } }, // 右下
		{ { -1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }, // 左上
		{ { -1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } }  // 左下
	};

	mesh.vertices = rectangleVertices;

	// インデックス割り当て
	for (size_t i = 0; i < mesh.vertices.size(); ++i) {
		mesh.indices.push_back(static_cast<uint32_t>(i));
	}

	if (doubleSided)
	{
		// 裏面生成（法線と巻き順を反転）
		std::vector<PrimitiveVertex> backVertices = rectangleVertices;
		
		// 法線反転
		for (auto& v : backVertices) {
			v.normal.z = -1.0f;
		}

		// 逆順登録して巻き順を反転させる
		for (auto it = backVertices.rbegin(); it != backVertices.rend(); ++it) {
			mesh.vertices.push_back(*it);
			mesh.indices.push_back(static_cast<uint32_t>(mesh.vertices.size() - 1));
		}
	}

	return mesh;
}

PrimitiveMesh PrimitiveGenerator::GenerateSphere(uint32_t segments, uint32_t rings)
{
	// 分割数設定
	const uint32_t kLatitudeDiv = rings;
	const uint32_t kLongitudeDiv = segments;
	const float kRadius = 1.0f;

	PrimitiveMesh mesh;

	// 三角形を直接構築
	for (uint32_t lat = 0; lat < kLatitudeDiv; ++lat)
	{
		float theta0 = float(lat) / float(kLatitudeDiv) * std::numbers::pi_v<float>;
		float theta1 = float(lat + 1) / float(kLatitudeDiv) * std::numbers::pi_v<float>;

		for (uint32_t lon = 0; lon < kLongitudeDiv; ++lon)
		{
			float phi0 = float(lon) / float(kLongitudeDiv) * 2.0f * std::numbers::pi_v<float>;
			float phi1 = float(lon + 1) / float(kLongitudeDiv) * 2.0f * std::numbers::pi_v<float>;

			// パッチの4頂点計算
			Vector3 p00 = {
				std::cos(phi0) * std::sin(theta0),
				std::cos(theta0),
				std::sin(phi0) * std::sin(theta0)
			};
			Vector3 p01 = {
				std::cos(phi1) * std::sin(theta0),
				std::cos(theta0),
				std::sin(phi1) * std::sin(theta0)
			};
			Vector3 p10 = {
				std::cos(phi0) * std::sin(theta1),
				std::cos(theta1),
				std::sin(phi0) * std::sin(theta1)
			};
			Vector3 p11 = {
				std::cos(phi1) * std::sin(theta1),
				std::cos(theta1),
				std::sin(phi1) * std::sin(theta1)
			};

			KCE::Vector2 uv00 = { float(lon) / float(kLongitudeDiv), float(lat) / float(kLatitudeDiv) };
			KCE::Vector2 uv01 = { float(lon + 1) / float(kLongitudeDiv), float(lat) / float(kLatitudeDiv) };
			KCE::Vector2 uv10 = { float(lon) / float(kLongitudeDiv), float(lat + 1) / float(kLatitudeDiv) };
			KCE::Vector2 uv11 = { float(lon + 1) / float(kLongitudeDiv), float(lat + 1) / float(kLatitudeDiv) };

			// 三角形1: p00 → p10 → p11
			mesh.vertices.push_back({ { p00.x * kRadius, p00.y * kRadius, p00.z * kRadius, 1.0f }, uv00, p00 });
			mesh.vertices.push_back({ { p10.x * kRadius, p10.y * kRadius, p10.z * kRadius, 1.0f }, uv10, p10 });
			mesh.vertices.push_back({ { p11.x * kRadius, p11.y * kRadius, p11.z * kRadius, 1.0f }, uv11, p11 });

			// 三角形2: p00 → p11 → p01
			mesh.vertices.push_back({ { p00.x * kRadius, p00.y * kRadius, p00.z * kRadius, 1.0f }, uv00, p00 });
			mesh.vertices.push_back({ { p11.x * kRadius, p11.y * kRadius, p11.z * kRadius, 1.0f }, uv11, p11 });
			mesh.vertices.push_back({ { p01.x * kRadius, p01.y * kRadius, p01.z * kRadius, 1.0f }, uv01, p01 });
		}
	}

	// Index生成
	for (size_t i = 0; i < mesh.vertices.size(); ++i) {
		mesh.indices.push_back(static_cast<uint32_t>(i));
	}

	return mesh;
}

PrimitiveMesh PrimitiveGenerator::GenerateCylinder(uint32_t segments, bool withCaps)
{
	const uint32_t kCylinderDivide = segments;
	const float kOuterRadius = 1.0f;
	const float kHeight = 2.0f;
	const float radianPerDiv = 2.0f * std::numbers::pi_v<float> / float(kCylinderDivide);

	PrimitiveMesh mesh;

	if (withCaps) {
		// 上面キャップ
		for (uint32_t index = 0; index < kCylinderDivide; ++index) {
			float sin0 = std::sin(radianPerDiv * index);
			float cos0 = std::cos(radianPerDiv * index);
			float sin1 = std::sin(radianPerDiv * (index + 1));
			float cos1 = std::cos(radianPerDiv * (index + 1));
			float u0 = float(index) / float(kCylinderDivide);
			float u1 = float(index + 1) / float(kCylinderDivide);
	
			// 三角形1: 中心 → 外側0 → 外側1
			mesh.vertices.push_back({ { 0.0f, kHeight / 2.0f, 0.0f, 1.0f }, { 0.5f, 0.5f }, { 0.0f, 1.0f, 0.0f } }); // 中心
			mesh.vertices.push_back({ { cos0 * kOuterRadius, kHeight / 2.0f, sin0 * kOuterRadius, 1.0f }, { u0, 0.0f }, { 0.0f, 1.0f, 0.0f } }); // 外側0
			mesh.vertices.push_back({ { cos1 * kOuterRadius, kHeight / 2.0f, sin1 * kOuterRadius, 1.0f }, { u1, 0.0f }, { 0.0f, 1.0f, 0.0f } }); // 外側1
		}
	
		// 下面キャップ
		for (uint32_t index = 0; index < kCylinderDivide; ++index) {
			float sin0 = std::sin(radianPerDiv * index);
			float cos0 = std::cos(radianPerDiv * index);
			float sin1 = std::sin(radianPerDiv * (index + 1));
			float cos1 = std::cos(radianPerDiv * (index + 1));
			float u0 = float(index) / float(kCylinderDivide);
			float u1 = float(index + 1) / float(kCylinderDivide);
	
			// 三角形1: 中心 → 外側1 → 外側0
			mesh.vertices.push_back({ { 0.0f, -kHeight / 2.0f, 0.0f, 1.0f }, { 0.5f, 0.5f }, { 0.0f, -1.0f, 0.0f } }); // 中心
			mesh.vertices.push_back({ { cos1 * kOuterRadius, -kHeight / 2.0f, sin1 * kOuterRadius, 1.0f }, { u1, 0.0f }, { 0.0f, -1.0f, 0.0f } }); // 外側1
			mesh.vertices.push_back({ { cos0 * kOuterRadius, -kHeight / 2.0f, sin0 * kOuterRadius, 1.0f }, { u0, 0.0f }, { 0.0f, -1.0f, 0.0f } }); // 外側0
		}
	}

	// 側面頂点
	for (uint32_t index = 0; index < kCylinderDivide; ++index)
	{
		float sin0 = std::sin(radianPerDiv * index);
		float cos0 = std::cos(radianPerDiv * index);
		float sin1 = std::sin(radianPerDiv * (index + 1));
		float cos1 = std::cos(radianPerDiv * (index + 1));
		float u0 = float(index) / float(kCylinderDivide);
		float u1 = float(index + 1) / float(kCylinderDivide);

		// 三角形1: 上外側0 → 上外側1 → 下外側0
		mesh.vertices.push_back({ { cos0 * kOuterRadius, kHeight / 2.0f, sin0 * kOuterRadius, 1.0f }, { u0, 0.0f }, { cos0, 0.0f, sin0 } });
		mesh.vertices.push_back({ { cos1 * kOuterRadius, kHeight / 2.0f, sin1 * kOuterRadius, 1.0f }, { u1, 0.0f }, { cos1, 0.0f, sin1 } });
		mesh.vertices.push_back({ { cos0 * kOuterRadius, -kHeight / 2.0f, sin0 * kOuterRadius, 1.0f }, { u0, 1.0f }, { cos0, 0.0f, sin0 } });

		// 三角形2: 上外側1 → 下外側1 → 下外側0
		mesh.vertices.push_back({ { cos1 * kOuterRadius, kHeight / 2.0f, sin1 * kOuterRadius, 1.0f }, { u1, 0.0f }, { cos1, 0.0f, sin1 } });
		mesh.vertices.push_back({ { cos1 * kOuterRadius, -kHeight / 2.0f, sin1 * kOuterRadius, 1.0f }, { u1, 1.0f }, { cos1, 0.0f, sin1 } });
		mesh.vertices.push_back({ { cos0 * kOuterRadius, -kHeight / 2.0f, sin0 * kOuterRadius, 1.0f }, { u0, 1.0f }, { cos0, 0.0f, sin0 } });
	}

	for (size_t i = 0; i < mesh.vertices.size(); ++i) {
		mesh.indices.push_back(static_cast<uint32_t>(i));
	}

	return mesh;
}

PrimitiveMesh PrimitiveGenerator::GenerateCone(uint32_t segments, bool withCap)
{
	PrimitiveMesh mesh;

	// パラメータ (Radius 1.0, Height 2.0)
	const uint32_t sliceCount = segments;
	const float radius = 1.0f;
	const float height = 2.0f;
	const float angleStep = 2.0f * std::numbers::pi_v<float> / static_cast<float>(sliceCount);

	Vector4 tip = { 0.0f, height, 0.0f, 1.0f };
	Vector4 center = { 0.0f, 0.0f, 0.0f, 1.0f };

	// 側面
	for (uint32_t i = 0; i < sliceCount; ++i)
	{
		float theta0 = angleStep * i;
		float theta1 = angleStep * (i + 1);

		Vector4 p0 = { radius * std::cos(theta0), 0.0f, radius * std::sin(theta0), 1.0f };
		Vector4 p1 = { radius * std::cos(theta1), 0.0f, radius * std::sin(theta1), 1.0f };

		// 法線（外積から計算）
		Vector3 a = { p0.x - tip.x, p0.y - tip.y, p0.z - tip.z };
		Vector3 b = { p1.x - tip.x, p1.y - tip.y, p1.z - tip.z };
		Vector3 normal = Vector3::Normalize(Vector3::Cross(b, a));

		mesh.vertices.push_back({ tip, {0.5f, 0.0f}, normal });
		mesh.vertices.push_back({ p1,  {1.0f, 1.0f}, normal });
		mesh.vertices.push_back({ p0,  {0.0f, 1.0f}, normal });
	}

	// 底面
	if (withCap) {
		Vector3 downNormal = { 0.0f, -1.0f, 0.0f };
		for (uint32_t i = 0; i < sliceCount; ++i)
		{
			float theta0 = angleStep * i;
			float theta1 = angleStep * (i + 1);
	
			Vector4 p0 = { radius * std::cos(theta0), 0.0f, radius * std::sin(theta0), 1.0f };
			Vector4 p1 = { radius * std::cos(theta1), 0.0f, radius * std::sin(theta1), 1.0f };
	
			KCE::Vector2 uvCenter = { 0.5f, 0.5f };
			KCE::Vector2 uv0 = { 0.5f + p0.x / (2.0f * radius), 0.5f + p0.z / (2.0f * radius) };
			KCE::Vector2 uv1 = { 0.5f + p1.x / (2.0f * radius), 0.5f + p1.z / (2.0f * radius) };
	
			mesh.vertices.push_back({ center, uvCenter, downNormal });
			mesh.vertices.push_back({ p0,     uv0,      downNormal });
			mesh.vertices.push_back({ p1,     uv1,      downNormal });
		}
	}

	for (size_t i = 0; i < mesh.vertices.size(); ++i) {
		mesh.indices.push_back(static_cast<uint32_t>(i));
	}

	return mesh;
}

PrimitiveMesh PrimitiveGenerator::GenerateRing(uint32_t segments, float innerRadius, float outerRadius)
{
	const uint32_t kRingDivide = segments;
	const float    kOuterRadius = outerRadius;
	const float    kInnerRadius = innerRadius;
	const float    radianPerDiv = 2.0f * std::numbers::pi_v<float> / float(kRingDivide);

	PrimitiveMesh mesh;

	for (uint32_t i = 0; i < kRingDivide; ++i)
	{
		// 0→1 の角度
		float theta0 = radianPerDiv * float(i);
		float theta1 = radianPerDiv * float(i + 1);

		// sin/cos
		float c0 = std::cos(theta0), s0 = std::sin(theta0);
		float c1 = std::cos(theta1), s1 = std::sin(theta1);

		// UV
		float u0 = float(i) / float(kRingDivide);
		float u1 = float(i + 1) / float(kRingDivide);

		// 三角形 1
		mesh.vertices.push_back({ { c0 * kOuterRadius, s0 * kOuterRadius, 0, 1 }, { u0, 0 }, { 0,0,1 } });
		mesh.vertices.push_back({ { c1 * kOuterRadius, s1 * kOuterRadius, 0, 1 }, { u1, 0 }, { 0,0,1 } });
		mesh.vertices.push_back({ { c0 * kInnerRadius, s0 * kInnerRadius, 0, 1 }, { u0, 1 }, { 0,0,1 } });

		// 三角形 2
		mesh.vertices.push_back({ { c1 * kOuterRadius, s1 * kOuterRadius, 0, 1 }, { u1, 0 }, { 0,0,1 } });
		mesh.vertices.push_back({ { c1 * kInnerRadius, s1 * kInnerRadius, 0, 1 }, { u1, 1 }, { 0,0,1 } });
		mesh.vertices.push_back({ { c0 * kInnerRadius, s0 * kInnerRadius, 0, 1 }, { u0, 1 }, { 0,0,1 } });
	}

	for (size_t i = 0; i < mesh.vertices.size(); ++i) {
		mesh.indices.push_back(static_cast<uint32_t>(i));
	}

	return mesh;
}

PrimitiveMesh PrimitiveGenerator::GenerateTorus(uint32_t segments, uint32_t tubeSegments, float tubeRadius)
{
	const uint32_t kCircleDiv = segments;
	const uint32_t kTubeDiv = tubeSegments;
	const float kOuterRadius = 1.0f;
	const float kInnerRadius = tubeRadius;

	PrimitiveMesh mesh;

	for (uint32_t i = 0; i < kCircleDiv; ++i)
	{
		float theta0 = float(i) / float(kCircleDiv) * 2.0f * std::numbers::pi_v<float>;
		float theta1 = float(i + 1) / float(kCircleDiv) * 2.0f * std::numbers::pi_v<float>;

		for (uint32_t j = 0; j < kTubeDiv; ++j)
		{
			float phi0 = float(j) / float(kTubeDiv) * 2.0f * std::numbers::pi_v<float>;
			float phi1 = float(j + 1) / float(kTubeDiv) * 2.0f * std::numbers::pi_v<float>;

			// 4頂点計算
			// p00
			Vector3 p00 = {
				(kOuterRadius + kInnerRadius * std::cos(phi0)) * std::cos(theta0),
				kInnerRadius * std::sin(phi0),
				(kOuterRadius + kInnerRadius * std::cos(phi0)) * std::sin(theta0)
			};
			// p01
			Vector3 p01 = {
				(kOuterRadius + kInnerRadius * std::cos(phi1)) * std::cos(theta0),
				kInnerRadius * std::sin(phi1),
				(kOuterRadius + kInnerRadius * std::cos(phi1)) * std::sin(theta0)
			};
			// p10
			Vector3 p10 = {
				(kOuterRadius + kInnerRadius * std::cos(phi0)) * std::cos(theta1),
				kInnerRadius * std::sin(phi0),
				(kOuterRadius + kInnerRadius * std::cos(phi0)) * std::sin(theta1)
			};
			// p11
			Vector3 p11 = {
				(kOuterRadius + kInnerRadius * std::cos(phi1)) * std::cos(theta1),
				kInnerRadius * std::sin(phi1),
				(kOuterRadius + kInnerRadius * std::cos(phi1)) * std::sin(theta1)
			};

			// 法線
			Vector3 center0 = { kOuterRadius * std::cos(theta0), 0.0f, kOuterRadius * std::sin(theta0) };
			Vector3 center1 = { kOuterRadius * std::cos(theta1), 0.0f, kOuterRadius * std::sin(theta1) };

			Vector3 n00 = p00 - center0;
			Vector3 n01 = p01 - center0;
			Vector3 n10 = p10 - center1;
			Vector3 n11 = p11 - center1;

			// UV
			KCE::Vector2 uv00 = { float(i) / float(kCircleDiv), float(j) / float(kTubeDiv) };
			KCE::Vector2 uv01 = { float(i) / float(kCircleDiv), float(j + 1) / float(kTubeDiv) };
			KCE::Vector2 uv10 = { float(i + 1) / float(kCircleDiv), float(j) / float(kTubeDiv) };
			KCE::Vector2 uv11 = { float(i + 1) / float(kCircleDiv), float(j + 1) / float(kTubeDiv) };

			// 三角形1: p00 → p10 → p11
			mesh.vertices.push_back({ { p00.x, p00.y, p00.z, 1.0f }, uv00, n00 });
			mesh.vertices.push_back({ { p10.x, p10.y, p10.z, 1.0f }, uv10, n10 });
			mesh.vertices.push_back({ { p11.x, p11.y, p11.z, 1.0f }, uv11, n11 });

			// 三角形2: p00 → p11 → p01
			mesh.vertices.push_back({ { p00.x, p00.y, p00.z, 1.0f }, uv00, n00 });
			mesh.vertices.push_back({ { p11.x, p11.y, p11.z, 1.0f }, uv11, n11 });
			mesh.vertices.push_back({ { p01.x, p01.y, p01.z, 1.0f }, uv01, n01 });
		}
	}

	for (size_t i = 0; i < mesh.vertices.size(); ++i) {
		mesh.indices.push_back(static_cast<uint32_t>(i));
	}

	return mesh;
}

PrimitiveMesh PrimitiveGenerator::GenerateCube(const PrimitiveOptions& options)
{
	PrimitiveMesh mesh;
	// 指定されたサイズを使用（そのままハーフサイズとして扱う）
	Vector3 s = options.cubeSize;

	// 前面 (Index 0)
	if (options.cubeFaceVisible[0])
	{
		uint32_t baseIndex = static_cast<uint32_t>(mesh.vertices.size());
		mesh.vertices.push_back({ { -s.x, -s.y,  s.z, 1.0f }, { 0, 1 }, { 0, 0, 1 } });
		mesh.vertices.push_back({ {  s.x, -s.y,  s.z, 1.0f }, { 1, 1 }, { 0, 0, 1 } });
		mesh.vertices.push_back({ {  s.x,  s.y,  s.z, 1.0f }, { 1, 0 }, { 0, 0, 1 } });
		mesh.vertices.push_back({ { -s.x,  s.y,  s.z, 1.0f }, { 0, 0 }, { 0, 0, 1 } });

		mesh.indices.push_back(baseIndex + 0);
		mesh.indices.push_back(baseIndex + 1);
		mesh.indices.push_back(baseIndex + 2);
		mesh.indices.push_back(baseIndex + 0);
		mesh.indices.push_back(baseIndex + 2);
		mesh.indices.push_back(baseIndex + 3);
	}

	// 後面 (Index 1)
	if (options.cubeFaceVisible[1])
	{
		uint32_t baseIndex = static_cast<uint32_t>(mesh.vertices.size());
		mesh.vertices.push_back({ {  s.x, -s.y, -s.z, 1.0f }, { 0, 1 }, { 0, 0, -1 } });
		mesh.vertices.push_back({ { -s.x, -s.y, -s.z, 1.0f }, { 1, 1 }, { 0, 0, -1 } });
		mesh.vertices.push_back({ { -s.x,  s.y, -s.z, 1.0f }, { 1, 0 }, { 0, 0, -1 } });
		mesh.vertices.push_back({ {  s.x,  s.y, -s.z, 1.0f }, { 0, 0 }, { 0, 0, -1 } });

		mesh.indices.push_back(baseIndex + 0);
		mesh.indices.push_back(baseIndex + 1);
		mesh.indices.push_back(baseIndex + 2);
		mesh.indices.push_back(baseIndex + 0);
		mesh.indices.push_back(baseIndex + 2);
		mesh.indices.push_back(baseIndex + 3);
	}

	// 上面 (Index 2)
	if (options.cubeFaceVisible[2])
	{
		uint32_t baseIndex = static_cast<uint32_t>(mesh.vertices.size());
		mesh.vertices.push_back({ { -s.x,  s.y,  s.z, 1.0f }, { 0, 1 }, { 0, 1, 0 } });
		mesh.vertices.push_back({ {  s.x,  s.y,  s.z, 1.0f }, { 1, 1 }, { 0, 1, 0 } });
		mesh.vertices.push_back({ {  s.x,  s.y, -s.z, 1.0f }, { 1, 0 }, { 0, 1, 0 } });
		mesh.vertices.push_back({ { -s.x,  s.y, -s.z, 1.0f }, { 0, 0 }, { 0, 1, 0 } });

		mesh.indices.push_back(baseIndex + 0);
		mesh.indices.push_back(baseIndex + 1);
		mesh.indices.push_back(baseIndex + 2);
		mesh.indices.push_back(baseIndex + 0);
		mesh.indices.push_back(baseIndex + 2);
		mesh.indices.push_back(baseIndex + 3);
	}

	// 下面 (Index 3)
	if (options.cubeFaceVisible[3])
	{
		uint32_t baseIndex = static_cast<uint32_t>(mesh.vertices.size());
		mesh.vertices.push_back({ { -s.x, -s.y, -s.z, 1.0f }, { 0, 1 }, { 0, -1, 0 } });
		mesh.vertices.push_back({ {  s.x, -s.y, -s.z, 1.0f }, { 1, 1 }, { 0, -1, 0 } });
		mesh.vertices.push_back({ {  s.x, -s.y,  s.z, 1.0f }, { 1, 0 }, { 0, -1, 0 } });
		mesh.vertices.push_back({ { -s.x, -s.y,  s.z, 1.0f }, { 0, 0 }, { 0, -1, 0 } });

		mesh.indices.push_back(baseIndex + 0);
		mesh.indices.push_back(baseIndex + 1);
		mesh.indices.push_back(baseIndex + 2);
		mesh.indices.push_back(baseIndex + 0);
		mesh.indices.push_back(baseIndex + 2);
		mesh.indices.push_back(baseIndex + 3);
	}

	// 右面 (Index 4)
	if (options.cubeFaceVisible[4])
	{
		uint32_t baseIndex = static_cast<uint32_t>(mesh.vertices.size());
		mesh.vertices.push_back({ {  s.x, -s.y,  s.z, 1.0f }, { 0, 1 }, { 1, 0, 0 } });
		mesh.vertices.push_back({ {  s.x, -s.y, -s.z, 1.0f }, { 1, 1 }, { 1, 0, 0 } });
		mesh.vertices.push_back({ {  s.x,  s.y, -s.z, 1.0f }, { 1, 0 }, { 1, 0, 0 } });
		mesh.vertices.push_back({ {  s.x,  s.y,  s.z, 1.0f }, { 0, 0 }, { 1, 0, 0 } });

		mesh.indices.push_back(baseIndex + 0);
		mesh.indices.push_back(baseIndex + 1);
		mesh.indices.push_back(baseIndex + 2);
		mesh.indices.push_back(baseIndex + 0);
		mesh.indices.push_back(baseIndex + 2);
		mesh.indices.push_back(baseIndex + 3);
	}

	// 左面 (Index 5)
	if (options.cubeFaceVisible[5])
	{
		uint32_t baseIndex = static_cast<uint32_t>(mesh.vertices.size());
		mesh.vertices.push_back({ { -s.x, -s.y, -s.z, 1.0f }, { 0, 1 }, { -1, 0, 0 } });
		mesh.vertices.push_back({ { -s.x, -s.y,  s.z, 1.0f }, { 1, 1 }, { -1, 0, 0 } });
		mesh.vertices.push_back({ { -s.x,  s.y,  s.z, 1.0f }, { 1, 0 }, { -1, 0, 0 } });
		mesh.vertices.push_back({ { -s.x,  s.y, -s.z, 1.0f }, { 0, 0 }, { -1, 0, 0 } });

		mesh.indices.push_back(baseIndex + 0);
		mesh.indices.push_back(baseIndex + 1);
		mesh.indices.push_back(baseIndex + 2);
		mesh.indices.push_back(baseIndex + 0);
		mesh.indices.push_back(baseIndex + 2);
		mesh.indices.push_back(baseIndex + 3);
	}

	return mesh;
}

PrimitiveMesh PrimitiveGenerator::GenerateStar(uint32_t points, float innerRadius)
{
	const int kPoints = points;
	const float outerRadius = 1.0f;

	PrimitiveMesh mesh;

	float angleStep = 2.0f * std::numbers::pi_v<float> / float(kPoints * 2);
	Vector3 normal = { 0.0f, 0.0f, 1.0f };

	// 頂点生成（外側・内側交互）
	std::vector<Vector4> starPoints;
	for (int i = 0; i < kPoints * 2; ++i)
	{
		float radius = (i % 2 == 0) ? outerRadius : innerRadius;
		float angle = angleStep * i;
		starPoints.push_back({ std::cos(angle) * radius, std::sin(angle) * radius, 0.0f, 1.0f });
	}

	Vector4 center = { 0.0f, 0.0f, 0.0f, 1.0f };
	KCE::Vector2 uvCenter = { 0.5f, 0.5f };

	// 扇状に分割
	for (int i = 0; i < kPoints * 2; ++i)
	{
		int nextIndex = (i + 1) % (kPoints * 2);

		Vector4 p0 = center;
		Vector4 p1 = starPoints[i];
		Vector4 p2 = starPoints[nextIndex];

		KCE::Vector2 uv0 = uvCenter;
		KCE::Vector2 uv1 = { (p1.x + 1.0f) * 0.5f, (p1.y + 1.0f) * 0.5f };
		KCE::Vector2 uv2 = { (p2.x + 1.0f) * 0.5f, (p2.y + 1.0f) * 0.5f };

		mesh.vertices.push_back({ p0, uv0, normal });
		mesh.vertices.push_back({ p1, uv1, normal });
		mesh.vertices.push_back({ p2, uv2, normal });
	}

	for (size_t i = 0; i < mesh.vertices.size(); ++i) {
		mesh.indices.push_back(static_cast<uint32_t>(i));
	}
	return mesh;
}

PrimitiveMesh PrimitiveGenerator::GenerateHeart(uint32_t segments)
{
	const int kDiv = segments;
	PrimitiveMesh mesh;

	PrimitiveVertex center = { { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f } };

	std::vector<PrimitiveVertex> outlineVertices;

	// 輪郭生成
	for (int i = 0; i <= kDiv; ++i)
	{
		float t = float(i) / float(kDiv) * 2.0f * std::numbers::pi_v<float>;
		
		// ハート数式
		float x = 16.0f * std::powf(std::sin(t), 3);
		float y = 13.0f * std::cosf(t) - 5.0f * std::cosf(2.0f * t) - 2.0f * std::cosf(3.0f * t) - std::cosf(4.0f * t);
		
		// スケーリング
		x /= 18.0f;
		y /= 18.0f;
		
		float u = (x + 1.0f) * 0.5f;
		float v = (y + 1.0f) * 0.5f;

		outlineVertices.push_back({ { x, y, 0.0f, 1.0f }, { u, v }, { 0.0f, 0.0f, 1.0f } });
	}

	// 三角形リスト構築
	for (int i = 0; i < kDiv; ++i)
	{
		mesh.vertices.push_back(center);
		mesh.vertices.push_back(outlineVertices[i]);
		mesh.vertices.push_back(outlineVertices[i + 1]);
	}

	for (size_t i = 0; i < mesh.vertices.size(); ++i) {
		mesh.indices.push_back(static_cast<uint32_t>(i));
	}
	return mesh;
}

PrimitiveMesh PrimitiveGenerator::GenerateSpiral(uint32_t segments, float turns)
{
	const int kLocalRingDiv = 16; 
	const int kHeightDiv = segments; 
	const float kRadius = 0.5f;
	const float kHeight = 1.0f;
	const float kTurns = turns;

	PrimitiveMesh mesh;

	std::vector<PrimitiveVertex> vertices;

	// 螺旋軌道の頂点生成
	for (int i = 0; i <= kHeightDiv; ++i)
	{
		float v = static_cast<float>(i) / static_cast<float>(kHeightDiv);
		float y = v * kHeight - kHeight * 0.5f;
		float angle = v * kTurns * 2.0f * std::numbers::pi_v<float>;
		float x = kRadius * std::cos(angle);
		float z = kRadius * std::sin(angle);
		float u = angle / (2.0f * std::numbers::pi_v<float>);
		while (u > 1.0f) u -= 1.0f; 

		float nx = x / kRadius;
		float nz = z / kRadius;

		PrimitiveVertex vertex;
		vertex.position = { x, y, z, 1.0f };
		vertex.texcoord = { u, v };
		vertex.normal = { nx, 0.0f, nz };
		vertices.push_back(vertex);
	}

	std::vector<PrimitiveVertex> tubeVertices;
	const float kTubeRadius = 0.05f;

	// チューブ生成
	for (int i = 0; i < vertices.size(); ++i)
	{
		Vector3 center = { vertices[i].position.x, vertices[i].position.y, vertices[i].position.z };

		// 進行方向
		Vector3 forward;
		if (i < vertices.size() - 1)
		{
			forward = {
				vertices[i + 1].position.x - center.x,
				vertices[i + 1].position.y - center.y,
				vertices[i + 1].position.z - center.z
			};
		}
		else
		{
			forward = {
				center.x - vertices[i - 1].position.x,
				center.y - vertices[i - 1].position.y,
				center.z - vertices[i - 1].position.z
			};
		}

		float length = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
		if (length > 0) forward = { forward.x / length, forward.y / length, forward.z / length };

		// 基準軸
		Vector3 up = { 0.0f, 1.0f, 0.0f };
		Vector3 right = {
			up.y * forward.z - up.z * forward.y,
			up.z * forward.x - up.x * forward.z,
			up.x * forward.y - up.y * forward.x
		};
		length = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
		if(length > 0) right = { right.x / length, right.y / length, right.z / length };

		up = {
			forward.y * right.z - forward.z * right.y,
			forward.z * right.x - forward.x * right.z,
			forward.x * right.y - forward.y * right.x
		};

		// 断面円
		for (int j = 0; j < kLocalRingDiv; ++j)
		{
			float angle = static_cast<float>(j) / static_cast<float>(kLocalRingDiv) * 2.0f * std::numbers::pi_v<float>;
			float cosA = std::cos(angle);
			float sinA = std::sin(angle);

			Vector3 tubePoint = {
				center.x + kTubeRadius * (right.x * cosA + up.x * sinA),
				center.y + kTubeRadius * (right.y * cosA + up.y * sinA),
				center.z + kTubeRadius * (right.z * cosA + up.z * sinA)
			};

			Vector3 normal = {
				tubePoint.x - center.x,
				tubePoint.y - center.y,
				tubePoint.z - center.z
			};
			length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
			if(length>0) normal = { normal.x / length, normal.y / length, normal.z / length };

			KCE::Vector2 texcoord = {
				static_cast<float>(j) / static_cast<float>(kLocalRingDiv),
				vertices[i].texcoord.y
			};

			PrimitiveVertex vertex;
			vertex.position = { tubePoint.x, tubePoint.y, tubePoint.z, 1.0f };
			vertex.texcoord = texcoord;
			vertex.normal = normal;
			tubeVertices.push_back(vertex);
		}
	}

	// 側面構成
	for (int i = 0; i < kHeightDiv; ++i)
	{
		for (int j = 0; j < kLocalRingDiv; ++j)
		{
			int current = i * kLocalRingDiv + j;
			int next = i * kLocalRingDiv + (j + 1) % kLocalRingDiv;
			int bottom = (i + 1) * kLocalRingDiv + j;
			int bottomNext = (i + 1) * kLocalRingDiv + (j + 1) % kLocalRingDiv;

			mesh.vertices.push_back(tubeVertices[current]);
			mesh.vertices.push_back(tubeVertices[next]);
			mesh.vertices.push_back(tubeVertices[bottom]);

			mesh.vertices.push_back(tubeVertices[next]);
			mesh.vertices.push_back(tubeVertices[bottomNext]);
			mesh.vertices.push_back(tubeVertices[bottom]);
		}
	}

	for (size_t i = 0; i < mesh.vertices.size(); ++i) {
		mesh.indices.push_back(static_cast<uint32_t>(i));
	}
	return mesh;
}
} // namespace KCE
