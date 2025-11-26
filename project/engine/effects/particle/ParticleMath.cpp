#include "ParticleMath.h"

#include <numbers>

namespace
{
	//===========================[ リング形状用定数 ]===========================//
	// リングの分割数
	constexpr uint32_t kRingDivide = 32;
	// リングの外側半径
	constexpr float kRingOuterRadius = 1.0f;
	// リングの内側半径
	constexpr float kRingInnerRadius = 0.2f;

	//===========================[ 円柱形状用定数 ]===========================//
	// 円柱の分割数
	constexpr uint32_t kCylinderDivide = 32;
	// 円柱の半径
	constexpr float kCylinderRadius = 1.0f;
	// 円柱の高さ
	constexpr float kCylinderHeight = 2.0f;

	//===========================[ 球体形状用定数 ]===========================//
	// 球体の緯度分割数
	constexpr uint32_t kSphereLatitudeDiv = 16;
	// 球体の経度分割数
	constexpr uint32_t kSphereLongitudeDiv = 32;
	// 球体の半径
	constexpr float kSphereRadius = 1.0f;

	//===========================[ トーラス形状用定数 ]===========================//
	// トーラスの外周分割数（大円）
	constexpr uint32_t kTorusCircleDiv = 32;
	// トーラスの断面分割数（小円）
	constexpr uint32_t kTorusTubeDiv = 16;
	// トーラスの大円半径
	constexpr float kTorusOuterRadius = 1.0f;
	// トーラスのチューブ半径
	constexpr float kTorusInnerRadius = 0.3f;

	//===========================[ 星形状用定数 ]===========================//
	// 星の頂点数（尖りの数）
	constexpr int kStarPoints = 5;
	// 星の外側半径
	constexpr float kStarOuterRadius = 1.0f;
	// 星の内側半径
	constexpr float kStarInnerRadius = 0.5f;

	//===========================[ ハート形状用定数 ]===========================//
	// ハートの分割数
	constexpr int kHeartDivide = 64;
	// ハートのスケール係数
	constexpr float kHeartScaleFactor = 18.0f;

	//===========================[ スパイラル形状用定数 ]===========================//
	// スパイラルの円周方向分割数
	constexpr int kSpiralRingDiv = 64;
	// スパイラルの高さ方向分割数
	constexpr int kSpiralHeightDiv = 32;
	// スパイラルの半径
	constexpr float kSpiralRadius = 0.5f;
	// スパイラルの高さ
	constexpr float kSpiralHeight = 1.0f;
	// スパイラルの巻き数
	constexpr float kSpiralTurns = 3.0f;
	// スパイラルのチューブ半径
	constexpr float kSpiralTubeRadius = 0.05f;

	//===========================[ 円錐形状用定数 ]===========================//
	// 円錐の分割数
	constexpr uint32_t kConeSliceCount = 32;
	// 円錐の底面半径
	constexpr float kConeRadius = 1.0f;
	// 円錐の高さ
	constexpr float kConeHeight = 2.0f;

	//===========================[ 立方体形状用定数 ]===========================//
	// 立方体の辺の長さ
	constexpr float kCubeSize = 1.0f;
}

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
	// 1セグメントあたりの角度を計算
	const float radianPerDiv = 2.0f * std::numbers::pi_v<float> / float(kRingDivide);

	std::vector<VertexData> ringVertices;
	// 1セグメントあたり6頂点（2三角形）
	ringVertices.reserve(kRingDivide * 6);

	// 各セグメントの頂点を生成
	for (uint32_t i = 0; i < kRingDivide; ++i)
	{
		// 現在と次のセグメントの角度を計算
		float theta0 = radianPerDiv * float(i);
		float theta1 = radianPerDiv * float(i + 1);

		// sin/cosを事前計算
		float c0 = std::cos(theta0), s0 = std::sin(theta0);
		float c1 = std::cos(theta1), s1 = std::sin(theta1);

		// テクスチャ座標U成分
		float u0 = float(i) / float(kRingDivide);
		float u1 = float(i + 1) / float(kRingDivide);

		// 三角形1: 外側0 → 外側1 → 内側0
		ringVertices.push_back({ { c0 * kRingOuterRadius, s0 * kRingOuterRadius, 0, 1 }, { u0, 0 }, { 0,0,1 } });
		ringVertices.push_back({ { c1 * kRingOuterRadius, s1 * kRingOuterRadius, 0, 1 }, { u1, 0 }, { 0,0,1 } });
		ringVertices.push_back({ { c0 * kRingInnerRadius, s0 * kRingInnerRadius, 0, 1 }, { u0, 1 }, { 0,0,1 } });

		// 三角形2: 外側1 → 内側1 → 内側0
		ringVertices.push_back({ { c1 * kRingOuterRadius, s1 * kRingOuterRadius, 0, 1 }, { u1, 0 }, { 0,0,1 } });
		ringVertices.push_back({ { c1 * kRingInnerRadius, s1 * kRingInnerRadius, 0, 1 }, { u1, 1 }, { 0,0,1 } });
		ringVertices.push_back({ { c0 * kRingInnerRadius, s0 * kRingInnerRadius, 0, 1 }, { u0, 1 }, { 0,0,1 } });
	}
	return ringVertices;
}

std::vector<VertexData> ParticleMath::MakeCylinderVertexData()
{
	// 1セグメントあたりの角度を計算
	const float radianPerDiv = 2.0f * std::numbers::pi_v<float> / float(kCylinderDivide);

	std::vector<VertexData> cylinderVertices;

	// 側面の頂点データを生成
	for (uint32_t index = 0; index < kCylinderDivide; ++index)
	{
		// sin/cosを計算
		float sin0 = std::sin(radianPerDiv * index);
		float cos0 = std::cos(radianPerDiv * index);
		float sin1 = std::sin(radianPerDiv * (index + 1));
		float cos1 = std::cos(radianPerDiv * (index + 1));
		// テクスチャ座標U成分
		float u0 = float(index) / float(kCylinderDivide);
		float u1 = float(index + 1) / float(kCylinderDivide);

		// 三角形1: 上外側0 → 上外側1 → 下外側0
		cylinderVertices.push_back({ { cos0 * kCylinderRadius, kCylinderHeight / 2.0f, sin0 * kCylinderRadius, 1.0f }, { u0, 0.0f }, { cos0, 0.0f, sin0 } });
		cylinderVertices.push_back({ { cos1 * kCylinderRadius, kCylinderHeight / 2.0f, sin1 * kCylinderRadius, 1.0f }, { u1, 0.0f }, { cos1, 0.0f, sin1 } });
		cylinderVertices.push_back({ { cos0 * kCylinderRadius, -kCylinderHeight / 2.0f, sin0 * kCylinderRadius, 1.0f }, { u0, 1.0f }, { cos0, 0.0f, sin0 } });

		// 三角形2: 上外側1 → 下外側1 → 下外側0
		cylinderVertices.push_back({ { cos1 * kCylinderRadius, kCylinderHeight / 2.0f, sin1 * kCylinderRadius, 1.0f }, { u1, 0.0f }, { cos1, 0.0f, sin1 } });
		cylinderVertices.push_back({ { cos1 * kCylinderRadius, -kCylinderHeight / 2.0f, sin1 * kCylinderRadius, 1.0f }, { u1, 1.0f }, { cos1, 0.0f, sin1 } });
		cylinderVertices.push_back({ { cos0 * kCylinderRadius, -kCylinderHeight / 2.0f, sin0 * kCylinderRadius, 1.0f }, { u0, 1.0f }, { cos0, 0.0f, sin0 } });
	}
	return cylinderVertices;
}

std::vector<VertexData> ParticleMath::MakeSphereVertexData()
{
	std::vector<VertexData> sphereVertices;

	// 三角形を直接構築（インデックスバッファを使用しない）
	for (uint32_t lat = 0; lat < kSphereLatitudeDiv; ++lat)
	{
		// 緯度方向の角度を計算（0〜π）
		float theta0 = float(lat) / float(kSphereLatitudeDiv) * std::numbers::pi_v<float>;
		float theta1 = float(lat + 1) / float(kSphereLatitudeDiv) * std::numbers::pi_v<float>;

		for (uint32_t lon = 0; lon < kSphereLongitudeDiv; ++lon)
		{
			// 経度方向の角度を計算（0〜2π）
			float phi0 = float(lon) / float(kSphereLongitudeDiv) * 2.0f * std::numbers::pi_v<float>;
			float phi1 = float(lon + 1) / float(kSphereLongitudeDiv) * 2.0f * std::numbers::pi_v<float>;

			// 4頂点の位置を計算（球面座標から直交座標への変換）
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

			// テクスチャ座標を計算
			Vector2 uv00 = { float(lon) / float(kSphereLongitudeDiv), float(lat) / float(kSphereLatitudeDiv) };
			Vector2 uv01 = { float(lon + 1) / float(kSphereLongitudeDiv), float(lat) / float(kSphereLatitudeDiv) };
			Vector2 uv10 = { float(lon) / float(kSphereLongitudeDiv), float(lat + 1) / float(kSphereLatitudeDiv) };
			Vector2 uv11 = { float(lon + 1) / float(kSphereLongitudeDiv), float(lat + 1) / float(kSphereLatitudeDiv) };

			// 三角形1: p00 → p10 → p11
			sphereVertices.push_back({ { p00.x * kSphereRadius, p00.y * kSphereRadius, p00.z * kSphereRadius, 1.0f }, uv00, p00 });
			sphereVertices.push_back({ { p10.x * kSphereRadius, p10.y * kSphereRadius, p10.z * kSphereRadius, 1.0f }, uv10, p10 });
			sphereVertices.push_back({ { p11.x * kSphereRadius, p11.y * kSphereRadius, p11.z * kSphereRadius, 1.0f }, uv11, p11 });

			// 三角形2: p00 → p11 → p01
			sphereVertices.push_back({ { p00.x * kSphereRadius, p00.y * kSphereRadius, p00.z * kSphereRadius, 1.0f }, uv00, p00 });
			sphereVertices.push_back({ { p11.x * kSphereRadius, p11.y * kSphereRadius, p11.z * kSphereRadius, 1.0f }, uv11, p11 });
			sphereVertices.push_back({ { p01.x * kSphereRadius, p01.y * kSphereRadius, p01.z * kSphereRadius, 1.0f }, uv01, p01 });
		}
	}
	return sphereVertices;
}

std::vector<VertexData> ParticleMath::MakeTorusVertexData()
{
	std::vector<VertexData> torusVertices;

	// 大円の各セグメントを処理
	for (uint32_t i = 0; i < kTorusCircleDiv; ++i)
	{
		// 大円方向の角度（0〜2π）
		float theta0 = float(i) / float(kTorusCircleDiv) * 2.0f * std::numbers::pi_v<float>;
		float theta1 = float(i + 1) / float(kTorusCircleDiv) * 2.0f * std::numbers::pi_v<float>;

		// 小円の各セグメントを処理
		for (uint32_t j = 0; j < kTorusTubeDiv; ++j)
		{
			// 小円方向の角度（0〜2π）
			float phi0 = float(j) / float(kTorusTubeDiv) * 2.0f * std::numbers::pi_v<float>;
			float phi1 = float(j + 1) / float(kTorusTubeDiv) * 2.0f * std::numbers::pi_v<float>;

			// 4頂点の位置を計算（トーラスのパラメトリック方程式）
			// p00: theta0, phi0
			Vector3 p00 = {
				(kTorusOuterRadius + kTorusInnerRadius * std::cos(phi0)) * std::cos(theta0),
				kTorusInnerRadius * std::sin(phi0),
				(kTorusOuterRadius + kTorusInnerRadius * std::cos(phi0)) * std::sin(theta0)
			};
			// p01: theta0, phi1
			Vector3 p01 = {
				(kTorusOuterRadius + kTorusInnerRadius * std::cos(phi1)) * std::cos(theta0),
				kTorusInnerRadius * std::sin(phi1),
				(kTorusOuterRadius + kTorusInnerRadius * std::cos(phi1)) * std::sin(theta0)
			};
			// p10: theta1, phi0
			Vector3 p10 = {
				(kTorusOuterRadius + kTorusInnerRadius * std::cos(phi0)) * std::cos(theta1),
				kTorusInnerRadius * std::sin(phi0),
				(kTorusOuterRadius + kTorusInnerRadius * std::cos(phi0)) * std::sin(theta1)
			};
			// p11: theta1, phi1
			Vector3 p11 = {
				(kTorusOuterRadius + kTorusInnerRadius * std::cos(phi1)) * std::cos(theta1),
				kTorusInnerRadius * std::sin(phi1),
				(kTorusOuterRadius + kTorusInnerRadius * std::cos(phi1)) * std::sin(theta1)
			};

			// 法線計算用：大円の中心位置
			Vector3 center0 = { kTorusOuterRadius * std::cos(theta0), 0.0f, kTorusOuterRadius * std::sin(theta0) };
			Vector3 center1 = { kTorusOuterRadius * std::cos(theta1), 0.0f, kTorusOuterRadius * std::sin(theta1) };

			// 法線ベクトル（頂点位置から大円中心を引いた方向）
			Vector3 n00 = p00 - center0;
			Vector3 n01 = p01 - center0;
			Vector3 n10 = p10 - center1;
			Vector3 n11 = p11 - center1;

			// テクスチャ座標
			Vector2 uv00 = { float(i) / float(kTorusCircleDiv), float(j) / float(kTorusTubeDiv) };
			Vector2 uv01 = { float(i) / float(kTorusCircleDiv), float(j + 1) / float(kTorusTubeDiv) };
			Vector2 uv10 = { float(i + 1) / float(kTorusCircleDiv), float(j) / float(kTorusTubeDiv) };
			Vector2 uv11 = { float(i + 1) / float(kTorusCircleDiv), float(j + 1) / float(kTorusTubeDiv) };

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
	std::vector<VertexData> vertices;

	// 1頂点あたりの角度（外側と内側合わせて10頂点）
	float angleStep = 2.0f * std::numbers::pi_v<float> / float(kStarPoints * 2);
	// Z軸正方向の法線
	Vector3 normal = { 0.0f, 0.0f, 1.0f };

	// 星の輪郭頂点を生成（外側と内側を交互に配置）
	std::vector<Vector4> starPoints;
	for (int i = 0; i < kStarPoints * 2; ++i)
	{
		// 偶数インデックスは外側、奇数は内側
		float radius = (i % 2 == 0) ? kStarOuterRadius : kStarInnerRadius;
		float angle = angleStep * i;
		starPoints.push_back({ std::cos(angle) * radius, std::sin(angle) * radius, 0.0f, 1.0f });
	}

	// 中心点とテクスチャ座標
	Vector4 center = { 0.0f, 0.0f, 0.0f, 1.0f };
	Vector2 uvCenter = { 0.5f, 0.5f };

	// 扇状に三角形を生成（中心 - 頂点i - 頂点i+1）
	for (int i = 0; i < kStarPoints * 2; ++i)
	{
		int nextIndex = (i + 1) % (kStarPoints * 2);

		Vector4 p0 = center;
		Vector4 p1 = starPoints[i];
		Vector4 p2 = starPoints[nextIndex];

		// テクスチャ座標を位置から計算
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
	std::vector<VertexData> vertices;

	// 中心点（原点）
	VertexData center = { { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f } };

	// 輪郭点を一時的に格納
	std::vector<VertexData> outlineVertices;

	// 反時計回りに輪郭の頂点を計算（ハートのパラメトリック方程式）
	for (int i = 0; i <= kHeartDivide; ++i)
	{
		// パラメータt（0〜2π）
		float t = float(i) / float(kHeartDivide) * 2.0f * std::numbers::pi_v<float>;
		// ハートのパラメトリック方程式
		float x = 16.0f * std::powf(std::sin(t), 3);
		float y = 13.0f * std::cosf(t) - 5.0f * std::cosf(2.0f * t) - 2.0f * std::cosf(3.0f * t) - std::cosf(4.0f * t);
		// スケールを調整して正規化
		x /= kHeartScaleFactor;
		y /= kHeartScaleFactor;
		// テクスチャ座標（0〜1の範囲にマッピング）
		float u = (x + 1.0f) * 0.5f;
		float v = (y + 1.0f) * 0.5f;

		outlineVertices.push_back({ { x, y, 0.0f, 1.0f }, { u, v }, { 0.0f, 0.0f, 1.0f } });
	}

	// 三角形リストを作成（中心と輪郭の2点で三角形を形成）
	for (int i = 0; i < kHeartDivide; ++i)
	{
		vertices.push_back(center);                  // 中心点
		vertices.push_back(outlineVertices[i]);      // 現在の輪郭点
		vertices.push_back(outlineVertices[i + 1]);  // 次の輪郭点
	}
	return vertices;
}

std::vector<VertexData> ParticleMath::MakeSpiralVertexData()
{
	std::vector<VertexData> vertices;

	// 螺旋の中心線上の頂点を計算
	for (int i = 0; i <= kSpiralHeightDiv; ++i)
	{
		// 高さパラメータ（0〜1）
		float v = static_cast<float>(i) / static_cast<float>(kSpiralHeightDiv);

		// 高さ位置（中心を原点にする）
		float y = v * kSpiralHeight - kSpiralHeight * 0.5f;

		// 回転角度（高さに応じて巻き数分回転）
		float angle = v * kSpiralTurns * 2.0f * std::numbers::pi_v<float>;

		// 円周上の位置を計算
		float x = kSpiralRadius * std::cos(angle);
		float z = kSpiralRadius * std::sin(angle);

		// テクスチャ座標（Uは角度から計算、0〜1に正規化）
		float u = angle / (2.0f * std::numbers::pi_v<float>);
		while (u > 1.0f) u -= 1.0f;

		// 法線ベクトル（外向き、半径方向）
		float nx = x / kSpiralRadius;
		float nz = z / kSpiralRadius;

		Vector4 position = { x, y, z, 1.0f };
		Vector2 texcoord = { u, v };
		Vector3 normal = { nx, 0.0f, nz };

		VertexData vertex;
		vertex.position = position;
		vertex.texcoord = texcoord;
		vertex.normal = normal;
		vertices.push_back(vertex);
	}

	// 螺旋をチューブ状の形状に変換
	std::vector<VertexData> tubeVertices;

	for (int i = 0; i < vertices.size(); ++i)
	{
		// 螺旋の中心線上の位置
		Vector3 center = { vertices[i].position.x, vertices[i].position.y, vertices[i].position.z };

		// 前方向ベクトル（螺旋の進行方向）を計算
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

		// 前方向ベクトルを正規化
		float length = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
		forward = { forward.x / length, forward.y / length, forward.z / length };

		// 上向きベクトル（仮）
		Vector3 up = { 0.0f, 1.0f, 0.0f };

		// 右向きベクトル（外積で計算）
		Vector3 right = {
			up.y * forward.z - up.z * forward.y,
			up.z * forward.x - up.x * forward.z,
			up.x * forward.y - up.y * forward.x
		};

		// 右向きベクトルを正規化
		length = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
		right = { right.x / length, right.y / length, right.z / length };

		// 実際の上向きベクトル（直交するように再計算）
		up = {
			forward.y * right.z - forward.z * right.y,
			forward.z * right.x - forward.x * right.z,
			forward.x * right.y - forward.y * right.x
		};

		// チューブの円周上に頂点を配置
		for (int j = 0; j < kSpiralRingDiv; ++j)
		{
			float angle = static_cast<float>(j) / static_cast<float>(kSpiralRingDiv) * 2.0f * std::numbers::pi_v<float>;
			float cosA = std::cos(angle);
			float sinA = std::sin(angle);

			// チューブ上の位置（中心からright/up方向にオフセット）
			Vector3 tubePoint = {
				center.x + kSpiralTubeRadius * (right.x * cosA + up.x * sinA),
				center.y + kSpiralTubeRadius * (right.y * cosA + up.y * sinA),
				center.z + kSpiralTubeRadius * (right.z * cosA + up.z * sinA)
			};

			// 法線ベクトル（中心から外向き）
			Vector3 normal = {
				tubePoint.x - center.x,
				tubePoint.y - center.y,
				tubePoint.z - center.z
			};

			// 法線を正規化
			length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
			normal = { normal.x / length, normal.y / length, normal.z / length };

			// テクスチャ座標
			Vector2 texcoord = {
				static_cast<float>(j) / static_cast<float>(kSpiralRingDiv),
				vertices[i].texcoord.y
			};

			VertexData vertex;
			vertex.position = { tubePoint.x, tubePoint.y, tubePoint.z, 1.0f };
			vertex.texcoord = texcoord;
			vertex.normal = normal;
			tubeVertices.push_back(vertex);
		}
	}

	// チューブ状の螺旋を三角形で構成
	std::vector<VertexData> finalVertices;

	for (int i = 0; i < kSpiralHeightDiv; ++i)
	{
		for (int j = 0; j < kSpiralRingDiv; ++j)
		{
			// インデックス計算
			int current = i * kSpiralRingDiv + j;
			int next = i * kSpiralRingDiv + (j + 1) % kSpiralRingDiv;
			int bottom = (i + 1) * kSpiralRingDiv + j;
			int bottomNext = (i + 1) * kSpiralRingDiv + (j + 1) % kSpiralRingDiv;

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

	// 1セグメントあたりの角度
	const float angleStep = 2.0f * std::numbers::pi_v<float> / static_cast<float>(kConeSliceCount);

	// 頂点と中心点
	Vector4 tip = { 0.0f, kConeHeight, 0.0f, 1.0f };
	Vector4 center = { 0.0f, 0.0f, 0.0f, 1.0f };

	// 側面の生成
	for (uint32_t i = 0; i < kConeSliceCount; ++i)
	{
		// セグメントの角度
		float theta0 = angleStep * i;
		float theta1 = angleStep * (i + 1);

		// 底面の2点を計算
		Vector4 p0 = { kConeRadius * std::cos(theta0), 0.0f, kConeRadius * std::sin(theta0), 1.0f };
		Vector4 p1 = { kConeRadius * std::cos(theta1), 0.0f, kConeRadius * std::sin(theta1), 1.0f };

		// 法線を外積から計算
		Vector3 a = { p0.x - tip.x, p0.y - tip.y, p0.z - tip.z };
		Vector3 b = { p1.x - tip.x, p1.y - tip.y, p1.z - tip.z };
		Vector3 normal = Vector3::Normalize(Vector3::Cross(b, a));

		// 側面の三角形
		vertices.push_back({ tip, {0.5f, 0.0f}, normal });
		vertices.push_back({ p1,  {1.0f, 1.0f}, normal });
		vertices.push_back({ p0,  {0.0f, 1.0f}, normal });
	}

	// 底面の生成
	Vector3 downNormal = { 0.0f, -1.0f, 0.0f };
	for (uint32_t i = 0; i < kConeSliceCount; ++i)
	{
		// セグメントの角度
		float theta0 = angleStep * i;
		float theta1 = angleStep * (i + 1);

		// 底面の2点
		Vector4 p0 = { kConeRadius * std::cos(theta0), 0.0f, kConeRadius * std::sin(theta0), 1.0f };
		Vector4 p1 = { kConeRadius * std::cos(theta1), 0.0f, kConeRadius * std::sin(theta1), 1.0f };

		// テクスチャ座標（円形にマッピング）
		Vector2 uvCenter = { 0.5f, 0.5f };
		Vector2 uv0 = { 0.5f + p0.x / (2.0f * kConeRadius), 0.5f + p0.z / (2.0f * kConeRadius) };
		Vector2 uv1 = { 0.5f + p1.x / (2.0f * kConeRadius), 0.5f + p1.z / (2.0f * kConeRadius) };

		// 底面の三角形
		vertices.push_back({ center, uvCenter, downNormal });
		vertices.push_back({ p0,     uv0,      downNormal });
		vertices.push_back({ p1,     uv1,      downNormal });
	}

	return vertices;
}

std::vector<VertexData> ParticleMath::MakeCubeVertexData()
{
	std::vector<VertexData> vertices;
	// 半幅（中心からの距離）
	const float h = kCubeSize * 0.5f;

	// 面を追加するラムダ関数
	// a:左上, b:左下, c:右上, d:右下（外側から見た順）
	auto addFace = [&](const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d, const Vector3& normal) {
		// 三角形1: a → b → c
		vertices.push_back({ { a.x, a.y, a.z, 1.0f }, { 0.0f, 0.0f }, normal });
		vertices.push_back({ { b.x, b.y, b.z, 1.0f }, { 0.0f, 1.0f }, normal });
		vertices.push_back({ { c.x, c.y, c.z, 1.0f }, { 1.0f, 0.0f }, normal });

		// 三角形2: c → b → d
		vertices.push_back({ { c.x, c.y, c.z, 1.0f }, { 1.0f, 0.0f }, normal });
		vertices.push_back({ { b.x, b.y, b.z, 1.0f }, { 0.0f, 1.0f }, normal });
		vertices.push_back({ { d.x, d.y, d.z, 1.0f }, { 1.0f, 1.0f }, normal });
		};

	// 前面 (+Z)
	addFace(
		Vector3{ -h,  h,  h }, // 左上
		Vector3{ -h, -h,  h }, // 左下
		Vector3{ h,  h,  h }, // 右上
		Vector3{ h, -h,  h }, // 右下
		Vector3{ 0.0f, 0.0f, 1.0f }
	);

	// 背面 (-Z)
	addFace(
		Vector3{ h,  h, -h }, // 左上（外側から見た場合）
		Vector3{ h, -h, -h }, // 左下
		Vector3{ -h,  h, -h }, // 右上
		Vector3{ -h, -h, -h }, // 右下
		Vector3{ 0.0f, 0.0f, -1.0f }
	);

	// 左面 (-X)
	addFace(
		Vector3{ -h,  h, -h }, // 左上
		Vector3{ -h, -h, -h }, // 左下
		Vector3{ -h,  h,  h }, // 右上
		Vector3{ -h, -h,  h }, // 右下
		Vector3{ -1.0f, 0.0f, 0.0f }
	);

	// 右面 (+X)
	addFace(
		Vector3{ h,  h,  h }, // 左上
		Vector3{ h, -h,  h }, // 左下
		Vector3{ h,  h, -h }, // 右上
		Vector3{ h, -h, -h }, // 右下
		Vector3{ 1.0f, 0.0f, 0.0f }
	);

	// 注：上面と下面はパーティクル用途では通常不要のためコメントアウト

	return vertices;
}