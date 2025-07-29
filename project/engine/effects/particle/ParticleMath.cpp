#include "ParticleMath.h"

#include <numbers>

std::vector<VertexData> ParticleMath::MakePlaneVertexData()
{
	// 頂点データを矩形で初期化
	std::vector<VertexData> rectangleVertices = {
	{ {  1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }, // 右上
	{ { -1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }, // 左上
	{ {  1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } }, // 右下
	{ {  1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } }, // 右下
	{ { -1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }, // 左上
	{ { -1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } }  // 左下
	};
	return rectangleVertices;
}

std::vector<VertexData> ParticleMath::MakeRingVertexData()
{
	// 頂点データをリングで初期化（triangle list）
	const uint32_t kRingDivide = 32;
	const float    kOuterRadius = 1.0f;
	const float    kInnerRadius = 0.2f;
	const float    radianPerDiv = 2.0f * std::numbers::pi_v<float> / float(kRingDivide);

	std::vector<VertexData> ringVertices;
	ringVertices.reserve(kRingDivide * 6);  // 1セグメントあたり6頂点

	for (uint32_t i = 0; i < kRingDivide; ++i)
	{
		// 0→1 の角度
		float theta0 = radianPerDiv * float(i);
		float theta1 = radianPerDiv * float(i + 1);

		// sin/cos をそれぞれ計算
		float c0 = std::cos(theta0), s0 = std::sin(theta0);
		float c1 = std::cos(theta1), s1 = std::sin(theta1);

		// テクスチャ座標 U
		float u0 = float(i) / float(kRingDivide);
		float u1 = float(i + 1) / float(kRingDivide);

		// ▽ 三角形 1： outer0 → outer1 → inner0
		ringVertices.push_back({ { c0 * kOuterRadius, s0 * kOuterRadius, 0, 1 }, { u0, 0 }, { 0,0,1 } });
		ringVertices.push_back({ { c1 * kOuterRadius, s1 * kOuterRadius, 0, 1 }, { u1, 0 }, { 0,0,1 } });
		ringVertices.push_back({ { c0 * kInnerRadius, s0 * kInnerRadius, 0, 1 }, { u0, 1 }, { 0,0,1 } });

		// ▽ 三角形 2： outer1 → inner1 → inner0
		ringVertices.push_back({ { c1 * kOuterRadius, s1 * kOuterRadius, 0, 1 }, { u1, 0 }, { 0,0,1 } });
		ringVertices.push_back({ { c1 * kInnerRadius, s1 * kInnerRadius, 0, 1 }, { u1, 1 }, { 0,0,1 } });
		ringVertices.push_back({ { c0 * kInnerRadius, s0 * kInnerRadius, 0, 1 }, { u0, 1 }, { 0,0,1 } });
	}
	return ringVertices;
}

std::vector<VertexData> ParticleMath::MakeCylinderVertexData()
{
	const uint32_t kCylinderDivide = 32; // 円柱の分割数
	const float kOuterRadius = 1.0f;     // 外側の半径
	const float kHeight = 2.0f;          // 円柱の高さ
	const float radianPerDiv = 2.0f * std::numbers::pi_v<float> / float(kCylinderDivide);

	std::vector<VertexData> cylinderVertices;

	//// 上面キャップの頂点データを生成
	//for (uint32_t index = 0; index < kCylinderDivide; ++index) {
	//	float sin0 = std::sin(radianPerDiv * index);
	//	float cos0 = std::cos(radianPerDiv * index);
	//	float sin1 = std::sin(radianPerDiv * (index + 1));
	//	float cos1 = std::cos(radianPerDiv * (index + 1));
	//	float u0 = float(index) / float(kCylinderDivide);
	//	float u1 = float(index + 1) / float(kCylinderDivide);

	//	// 三角形1: 中心 → 外側0 → 外側1
	//	cylinderVertices.push_back({ { 0.0f, kHeight / 2.0f, 0.0f, 1.0f }, { 0.5f, 0.5f }, { 0.0f, 1.0f, 0.0f } }); // 中心
	//	cylinderVertices.push_back({ { cos0 * kOuterRadius, kHeight / 2.0f, sin0 * kOuterRadius, 1.0f }, { u0, 0.0f }, { 0.0f, 1.0f, 0.0f } }); // 外側0
	//	cylinderVertices.push_back({ { cos1 * kOuterRadius, kHeight / 2.0f, sin1 * kOuterRadius, 1.0f }, { u1, 0.0f }, { 0.0f, 1.0f, 0.0f } }); // 外側1
	//}

	//// 下面キャップの頂点データを生成
	//for (uint32_t index = 0; index < kCylinderDivide; ++index) {
	//	float sin0 = std::sin(radianPerDiv * index);
	//	float cos0 = std::cos(radianPerDiv * index);
	//	float sin1 = std::sin(radianPerDiv * (index + 1));
	//	float cos1 = std::cos(radianPerDiv * (index + 1));
	//	float u0 = float(index) / float(kCylinderDivide);
	//	float u1 = float(index + 1) / float(kCylinderDivide);

	//	// 三角形1: 中心 → 外側1 → 外側0
	//	cylinderVertices.push_back({ { 0.0f, -kHeight / 2.0f, 0.0f, 1.0f }, { 0.5f, 0.5f }, { 0.0f, -1.0f, 0.0f } }); // 中心
	//	cylinderVertices.push_back({ { cos1 * kOuterRadius, -kHeight / 2.0f, sin1 * kOuterRadius, 1.0f }, { u1, 0.0f }, { 0.0f, -1.0f, 0.0f } }); // 外側1
	//	cylinderVertices.push_back({ { cos0 * kOuterRadius, -kHeight / 2.0f, sin0 * kOuterRadius, 1.0f }, { u0, 0.0f }, { 0.0f, -1.0f, 0.0f } }); // 外側0
	//}

	// 側面の頂点データを生成
	for (uint32_t index = 0; index < kCylinderDivide; ++index)
	{
		float sin0 = std::sin(radianPerDiv * index);
		float cos0 = std::cos(radianPerDiv * index);
		float sin1 = std::sin(radianPerDiv * (index + 1));
		float cos1 = std::cos(radianPerDiv * (index + 1));
		float u0 = float(index) / float(kCylinderDivide);
		float u1 = float(index + 1) / float(kCylinderDivide);

		// 三角形1: 上外側0 → 上外側1 → 下外側0
		cylinderVertices.push_back({ { cos0 * kOuterRadius, kHeight / 2.0f, sin0 * kOuterRadius, 1.0f }, { u0, 0.0f }, { cos0, 0.0f, sin0 } });
		cylinderVertices.push_back({ { cos1 * kOuterRadius, kHeight / 2.0f, sin1 * kOuterRadius, 1.0f }, { u1, 0.0f }, { cos1, 0.0f, sin1 } });
		cylinderVertices.push_back({ { cos0 * kOuterRadius, -kHeight / 2.0f, sin0 * kOuterRadius, 1.0f }, { u0, 1.0f }, { cos0, 0.0f, sin0 } });

		// 三角形2: 上外側1 → 下外側1 → 下外側0
		cylinderVertices.push_back({ { cos1 * kOuterRadius, kHeight / 2.0f, sin1 * kOuterRadius, 1.0f }, { u1, 0.0f }, { cos1, 0.0f, sin1 } });
		cylinderVertices.push_back({ { cos1 * kOuterRadius, -kHeight / 2.0f, sin1 * kOuterRadius, 1.0f }, { u1, 1.0f }, { cos1, 0.0f, sin1 } });
		cylinderVertices.push_back({ { cos0 * kOuterRadius, -kHeight / 2.0f, sin0 * kOuterRadius, 1.0f }, { u0, 1.0f }, { cos0, 0.0f, sin0 } });
	}
	return cylinderVertices;
}

std::vector<VertexData> ParticleMath::MakeSphereVertexData()
{
	const uint32_t kLatitudeDiv = 16;
	const uint32_t kLongitudeDiv = 32;
	const float kRadius = 1.0f;

	std::vector<VertexData> sphereVertices;

	// 三角形を直接構築（インデックスバッファなし）
	for (uint32_t lat = 0; lat < kLatitudeDiv; ++lat)
	{
		float theta0 = float(lat) / float(kLatitudeDiv) * std::numbers::pi_v<float>;
		float theta1 = float(lat + 1) / float(kLatitudeDiv) * std::numbers::pi_v<float>;

		for (uint32_t lon = 0; lon < kLongitudeDiv; ++lon)
		{
			float phi0 = float(lon) / float(kLongitudeDiv) * 2.0f * std::numbers::pi_v<float>;
			float phi1 = float(lon + 1) / float(kLongitudeDiv) * 2.0f * std::numbers::pi_v<float>;

			// 4頂点計算（パッチ1枚分）
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

			Vector2 uv00 = { float(lon) / float(kLongitudeDiv), float(lat) / float(kLatitudeDiv) };
			Vector2 uv01 = { float(lon + 1) / float(kLongitudeDiv), float(lat) / float(kLatitudeDiv) };
			Vector2 uv10 = { float(lon) / float(kLongitudeDiv), float(lat + 1) / float(kLatitudeDiv) };
			Vector2 uv11 = { float(lon + 1) / float(kLongitudeDiv), float(lat + 1) / float(kLatitudeDiv) };

			// 三角形1: p00 → p10 → p11
			sphereVertices.push_back({ { p00.x * kRadius, p00.y * kRadius, p00.z * kRadius, 1.0f }, uv00, p00 });
			sphereVertices.push_back({ { p10.x * kRadius, p10.y * kRadius, p10.z * kRadius, 1.0f }, uv10, p10 });
			sphereVertices.push_back({ { p11.x * kRadius, p11.y * kRadius, p11.z * kRadius, 1.0f }, uv11, p11 });

			// 三角形2: p00 → p11 → p01
			sphereVertices.push_back({ { p00.x * kRadius, p00.y * kRadius, p00.z * kRadius, 1.0f }, uv00, p00 });
			sphereVertices.push_back({ { p11.x * kRadius, p11.y * kRadius, p11.z * kRadius, 1.0f }, uv11, p11 });
			sphereVertices.push_back({ { p01.x * kRadius, p01.y * kRadius, p01.z * kRadius, 1.0f }, uv01, p01 });
		}
	}
	return sphereVertices;
}

std::vector<VertexData> ParticleMath::MakeTorusVertexData()
{
	const uint32_t kCircleDiv = 32;     // ドーナツの外周分割数（大円）
	const uint32_t kTubeDiv = 16;       // チューブの断面分割数（小円）
	const float kOuterRadius = 1.0f;    // 大円の半径
	const float kInnerRadius = 0.3f;    // チューブ（小円）の半径

	std::vector<VertexData> torusVertices;

	for (uint32_t i = 0; i < kCircleDiv; ++i)
	{
		float theta0 = float(i) / float(kCircleDiv) * 2.0f * std::numbers::pi_v<float>;
		float theta1 = float(i + 1) / float(kCircleDiv) * 2.0f * std::numbers::pi_v<float>;

		for (uint32_t j = 0; j < kTubeDiv; ++j)
		{
			float phi0 = float(j) / float(kTubeDiv) * 2.0f * std::numbers::pi_v<float>;
			float phi1 = float(j + 1) / float(kTubeDiv) * 2.0f * std::numbers::pi_v<float>;

			// 4頂点の計算（トーラスは大円＋小円の組み合わせ）
			// p00 - theta0, phi0
			Vector3 p00 = {
				(kOuterRadius + kInnerRadius * std::cos(phi0)) * std::cos(theta0),
				kInnerRadius * std::sin(phi0),
				(kOuterRadius + kInnerRadius * std::cos(phi0)) * std::sin(theta0)
			};
			// p01 - theta0, phi1
			Vector3 p01 = {
				(kOuterRadius + kInnerRadius * std::cos(phi1)) * std::cos(theta0),
				kInnerRadius * std::sin(phi1),
				(kOuterRadius + kInnerRadius * std::cos(phi1)) * std::sin(theta0)
			};
			// p10 - theta1, phi0
			Vector3 p10 = {
				(kOuterRadius + kInnerRadius * std::cos(phi0)) * std::cos(theta1),
				kInnerRadius * std::sin(phi0),
				(kOuterRadius + kInnerRadius * std::cos(phi0)) * std::sin(theta1)
			};
			// p11 - theta1, phi1
			Vector3 p11 = {
				(kOuterRadius + kInnerRadius * std::cos(phi1)) * std::cos(theta1),
				kInnerRadius * std::sin(phi1),
				(kOuterRadius + kInnerRadius * std::cos(phi1)) * std::sin(theta1)
			};

			// 法線は中心からの方向（頂点位置 - 大円中心）
			// 大円中心はtheta軸上の点(kOuterRadius * cos(theta), 0, kOuterRadius * sin(theta))
			Vector3 center0 = { kOuterRadius * std::cos(theta0), 0.0f, kOuterRadius * std::sin(theta0) };
			Vector3 center1 = { kOuterRadius * std::cos(theta1), 0.0f, kOuterRadius * std::sin(theta1) };

			Vector3 n00 = p00 - center0;
			Vector3 n01 = p01 - center0;
			Vector3 n10 = p10 - center1;
			Vector3 n11 = p11 - center1;

			// UV座標
			Vector2 uv00 = { float(i) / float(kCircleDiv), float(j) / float(kTubeDiv) };
			Vector2 uv01 = { float(i) / float(kCircleDiv), float(j + 1) / float(kTubeDiv) };
			Vector2 uv10 = { float(i + 1) / float(kCircleDiv), float(j) / float(kTubeDiv) };
			Vector2 uv11 = { float(i + 1) / float(kCircleDiv), float(j + 1) / float(kTubeDiv) };

			// 三角形1: p00 → p10 → p11
			torusVertices.push_back({ { p00.x, p00.y, p00.z, 1.0f }, uv00, n00 });
			torusVertices.push_back({ { p10.x, p10.y, p10.z, 1.0f }, uv10, n10 });
			torusVertices.push_back({ { p11.x, p11.y, p11.z, 1.0f }, uv11, n11 });

			// 三角形2: p00 → p11 → p01
			torusVertices.push_back({ { p00.x, p00.y, p00.z, 1.0f }, uv00, n00 });
			torusVertices.push_back({ { p11.x, p11.y, p11.z, 1.0f }, uv11, n11 });
			torusVertices.push_back({ { p01.x, p01.y, p01.z, 1.0f }, uv01, n01 });
		}
	}
	return torusVertices;
}

std::vector<VertexData> ParticleMath::MakeStarVertexData()
{
	const int kPoints = 5; // 星の頂点数（5つの尖り）
	const float outerRadius = 1.0f;  // 星の外側の半径
	const float innerRadius = 0.5f;  // 星の内側の半径

	std::vector<VertexData> vertices;

	float angleStep = 2.0f * std::numbers::pi_v<float> / float(kPoints * 2); // 外内合わせて10頂点
	Vector3 normal = { 0.0f, 0.0f, 1.0f };

	// 星の頂点を交互に生成（外側、内側、外側、内側...）
	std::vector<Vector4> starPoints;
	for (int i = 0; i < kPoints * 2; ++i)
	{
		float radius = (i % 2 == 0) ? outerRadius : innerRadius;
		float angle = angleStep * i;
		starPoints.push_back({ std::cos(angle) * radius, std::sin(angle) * radius, 0.0f, 1.0f });
	}

	Vector4 center = { 0.0f, 0.0f, 0.0f, 1.0f };
	Vector2 uvCenter = { 0.5f, 0.5f };

	// 扇状に三角形を分割（中心 - 頂点 i - 頂点 i+1）
	for (int i = 0; i < kPoints * 2; ++i)
	{
		int nextIndex = (i + 1) % (kPoints * 2);

		Vector4 p0 = center;
		Vector4 p1 = starPoints[i];
		Vector4 p2 = starPoints[nextIndex];

		Vector2 uv0 = uvCenter;
		Vector2 uv1 = { (p1.x + 1.0f) * 0.5f, (p1.y + 1.0f) * 0.5f };
		Vector2 uv2 = { (p2.x + 1.0f) * 0.5f, (p2.y + 1.0f) * 0.5f };

		vertices.push_back({ p0, uv0, normal });
		vertices.push_back({ p1, uv1, normal });
		vertices.push_back({ p2, uv2, normal });
	}
	return vertices;
}

std::vector<VertexData> ParticleMath::MakeHeartVertexData()
{
	const int kDiv = 64;
	std::vector<VertexData> vertices;

	// 中心点（原点）
	VertexData center = { { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f } };

	// 輪郭点の配列を一時的に保存
	std::vector<VertexData> outlineVertices;

	// 反時計回りに輪郭の頂点を計算
	for (int i = 0; i <= kDiv; ++i)
	{
		float t = float(i) / float(kDiv) * 2.0f * std::numbers::pi_v<float>;
		// ハートのパラメトリック方程式
		float x = 16.0f * std::powf(std::sin(t), 3);
		float y = 13.0f * std::cosf(t) - 5.0f * std::cosf(2.0f * t) - 2.0f * std::cosf(3.0f * t) - std::cosf(4.0f * t);
		// 適切なスケーリング（正規化）
		x /= 18.0f;
		y /= 18.0f;
		// テクスチャ座標（0〜1の範囲にマッピング）
		float u = (x + 1.0f) * 0.5f;
		float v = (y + 1.0f) * 0.5f;

		outlineVertices.push_back({ { x, y, 0.0f, 1.0f }, { u, v }, { 0.0f, 0.0f, 1.0f } });
	}

	// 三角形リストを作成（中心と輪郭の2点で三角形を形成）
	for (int i = 0; i < kDiv; ++i)
	{
		// 各三角形は中心点、現在の輪郭点、次の輪郭点からなる
		vertices.push_back(center); // 中心点
		vertices.push_back(outlineVertices[i]); // 現在の輪郭点
		vertices.push_back(outlineVertices[i + 1]); // 次の輪郭点
	}
	return vertices;
}

std::vector<VertexData> ParticleMath::MakeSpiralVertexData()
{
	const int kRingDiv = 64;   // 円周方向の分割数
	const int kHeightDiv = 32; // 高さ方向の分割数
	const float kRadius = 0.5f; // 螺旋の半径
	const float kHeight = 1.0f; // 螺旋の高さ
	const float kTurns = 3.0f;  // 螺旋の巻き数

	std::vector<VertexData> vertices;

	// 螺旋の軌道上に頂点を配置
	for (int i = 0; i <= kHeightDiv; ++i)
	{
		// 高さパラメータ (0 から 1)
		float v = static_cast<float>(i) / static_cast<float>(kHeightDiv);

		// 高さ位置
		float y = v * kHeight - kHeight * 0.5f; // 中心を原点にするため -kHeight/2 から +kHeight/2 の範囲に

		// 回転角度（高さに応じて kTurns 回転する）
		float angle = v * kTurns * 2.0f * std::numbers::pi_v<float>;

		// 円周上の位置
		float x = kRadius * std::cos(angle);
		float z = kRadius * std::sin(angle);

		// テクスチャ座標
		float u = angle / (2.0f * std::numbers::pi_v<float>);
		while (u > 1.0f) u -= 1.0f; // 0-1の範囲に正規化

		// 法線ベクトルは外向き（簡易的に半径方向）
		float nx = x / kRadius;
		float nz = z / kRadius;

		// Vector4, Vector2, Vector3を使用
		Vector4 position = { x, y, z, 1.0f };
		Vector2 texcoord = { u, v };
		Vector3 normal = { nx, 0.0f, nz };

		VertexData vertex;
		vertex.position = position;
		vertex.texcoord = texcoord;
		vertex.normal = normal;
		vertices.push_back(vertex);
	}

	// これで単純な螺旋の点列ができましたが、これを太さを持った形状にする場合は、
	// 各点から法線方向に複数の頂点を配置し、三角形を形成する必要があります。

	// 太さを持った螺旋（チューブ状）にするためのコード
	std::vector<VertexData> tubeVertices;
	const float kTubeRadius = 0.05f; // チューブの半径

	for (int i = 0; i < vertices.size(); ++i)
	{
		// 螺旋の中心線上の位置
		Vector3 center = { vertices[i].position.x, vertices[i].position.y, vertices[i].position.z };

		// 前方向ベクトル（螺旋の進行方向）
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

		// 正規化
		float length = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
		forward = { forward.x / length, forward.y / length, forward.z / length };

		// 上向きベクトル（仮の上方向）
		Vector3 up = { 0.0f, 1.0f, 0.0f };

		// 右向きベクトル（外向き）
		Vector3 right = {
			up.y * forward.z - up.z * forward.y,
			up.z * forward.x - up.x * forward.z,
			up.x * forward.y - up.y * forward.x
		};

		// 正規化
		length = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
		right = { right.x / length, right.y / length, right.z / length };

		// 実際の上向きベクトル（直交するように）
		up = {
			forward.y * right.z - forward.z * right.y,
			forward.z * right.x - forward.x * right.z,
			forward.x * right.y - forward.y * right.x
		};

		// 螺旋の周りに円形の頂点を配置
		for (int j = 0; j < kRingDiv; ++j)
		{
			float angle = static_cast<float>(j) / static_cast<float>(kRingDiv) * 2.0f * std::numbers::pi_v<float>;
			float cosA = std::cos(angle);
			float sinA = std::sin(angle);

			// チューブ上の位置（中心から right と up 方向に offset）
			Vector3 tubePoint = {
				center.x + kTubeRadius * (right.x * cosA + up.x * sinA),
				center.y + kTubeRadius * (right.y * cosA + up.y * sinA),
				center.z + kTubeRadius * (right.z * cosA + up.z * sinA)
			};

			// 法線ベクトル（中心から外向き）
			Vector3 normal = {
				tubePoint.x - center.x,
				tubePoint.y - center.y,
				tubePoint.z - center.z
			};

			// 正規化
			length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
			normal = { normal.x / length, normal.y / length, normal.z / length };

			// テクスチャ座標
			Vector2 texcoord = {
				static_cast<float>(j) / static_cast<float>(kRingDiv),
				vertices[i].texcoord.y
			};

			VertexData vertex;
			vertex.position = { tubePoint.x, tubePoint.y, tubePoint.z, 1.0f }; // Vector4
			vertex.texcoord = texcoord;
			vertex.normal = normal;
			tubeVertices.push_back(vertex);
		}
	}

	// チューブ状の螺旋を三角形で構成
	std::vector<VertexData> finalVertices;

	for (int i = 0; i < kHeightDiv; ++i)
	{
		for (int j = 0; j < kRingDiv; ++j)
		{
			int current = i * kRingDiv + j;
			int next = i * kRingDiv + (j + 1) % kRingDiv;
			int bottom = (i + 1) * kRingDiv + j;
			int bottomNext = (i + 1) * kRingDiv + (j + 1) % kRingDiv;

			// 上部の三角形
			finalVertices.push_back(tubeVertices[current]);
			finalVertices.push_back(tubeVertices[next]);
			finalVertices.push_back(tubeVertices[bottom]);

			// 下部の三角形
			finalVertices.push_back(tubeVertices[next]);
			finalVertices.push_back(tubeVertices[bottomNext]);
			finalVertices.push_back(tubeVertices[bottom]);
		}
	}
	return finalVertices;
}

std::vector<VertexData> ParticleMath::MakeConeVertexData()
{
	std::vector<VertexData> vertices;

	// 固定パラメータ
	const uint32_t sliceCount = 32;
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

		vertices.push_back({ tip, {0.5f, 0.0f}, normal });
		vertices.push_back({ p1,  {1.0f, 1.0f}, normal });
		vertices.push_back({ p0,  {0.0f, 1.0f}, normal });
	}

	// 底面
	Vector3 downNormal = { 0.0f, -1.0f, 0.0f };
	for (uint32_t i = 0; i < sliceCount; ++i)
	{
		float theta0 = angleStep * i;
		float theta1 = angleStep * (i + 1);

		Vector4 p0 = { radius * std::cos(theta0), 0.0f, radius * std::sin(theta0), 1.0f };
		Vector4 p1 = { radius * std::cos(theta1), 0.0f, radius * std::sin(theta1), 1.0f };

		Vector2 uvCenter = { 0.5f, 0.5f };
		Vector2 uv0 = { 0.5f + p0.x / (2.0f * radius), 0.5f + p0.z / (2.0f * radius) };
		Vector2 uv1 = { 0.5f + p1.x / (2.0f * radius), 0.5f + p1.z / (2.0f * radius) };

		vertices.push_back({ center, uvCenter, downNormal });
		vertices.push_back({ p0,     uv0,      downNormal });
		vertices.push_back({ p1,     uv1,      downNormal });
	}

	return vertices;
}
