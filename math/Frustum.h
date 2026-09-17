#pragma once

#include <array>
#include <cmath>

#include "math/AABB.h"
#include "math/MatrixFunc.h"
#include "math/Sphere.h"

namespace KCE
{
/** @brief 法線が内側を向く平面。点 p との符号付き距離は dot(normal, p) + distance */
struct Plane
{
	Vector3 normal{};
	float distance = 0.0f;
};

/** @brief ビューの内側を6枚の平面で表す視錐台 */
struct Frustum
{
	// 左・右・下・上・近・遠。法線は内側向き
	std::array<Plane, 6> planes;

	/** @brief ビュー×プロジェクション行列から作る（このエンジンは行ベクトル・row_major） */
	static Frustum FromViewProjection(const Matrix4x4& viewProjection)
	{
		// 行ベクトルではクリップ座標の各成分は行列の列との内積になる。
		// DirectX の範囲 -w<=x<=w、-w<=y<=w、0<=z<=w を平面へ展開する。
		Frustum result;
		result.planes = {
			MakePlane(viewProjection, 3, 0, 1.0f),
			MakePlane(viewProjection, 3, 0, -1.0f),
			MakePlane(viewProjection, 3, 1, 1.0f),
			MakePlane(viewProjection, 3, 1, -1.0f),
			MakeColumnPlane(viewProjection, 2),
			MakePlane(viewProjection, 3, 2, -1.0f),
		};
		return result;
	}

	/** @brief AABB が少しでも視錐台に入っていれば真（完全に外なら偽） */
	bool Intersects(const AABB& aabb) const
	{
		for (const Plane& plane : planes)
		{
			const Vector3 positive = {
				plane.normal.x >= 0.0f ? aabb.max_.x : aabb.min_.x,
				plane.normal.y >= 0.0f ? aabb.max_.y : aabb.min_.y,
				plane.normal.z >= 0.0f ? aabb.max_.z : aabb.min_.z,
			};
			if (Vector3::Dot(plane.normal, positive) + plane.distance < 0.0f)
			{
				return false;
			}
		}
		return true;
	}

	/** @brief 球が少しでも視錐台に入っていれば真 */
	bool Intersects(const Sphere& sphere) const
	{
		for (const Plane& plane : planes)
		{
			if (Vector3::Dot(plane.normal, sphere.center) + plane.distance < -sphere.radius)
			{
				return false;
			}
		}
		return true;
	}

private:
	static Plane NormalizePlane(float x, float y, float z, float distance)
	{
		const float length = std::sqrt(x * x + y * y + z * z);
		if (length == 0.0f)
		{
			return {};
		}
		return { { x / length, y / length, z / length }, distance / length };
	}

	static Plane MakeColumnPlane(const Matrix4x4& matrix, uint32_t column)
	{
		return NormalizePlane(matrix.m[0][column], matrix.m[1][column], matrix.m[2][column], matrix.m[3][column]);
	}

	static Plane MakePlane(const Matrix4x4& matrix, uint32_t firstColumn, uint32_t secondColumn, float secondSign)
	{
		return NormalizePlane(
			matrix.m[0][firstColumn] + matrix.m[0][secondColumn] * secondSign,
			matrix.m[1][firstColumn] + matrix.m[1][secondColumn] * secondSign,
			matrix.m[2][firstColumn] + matrix.m[2][secondColumn] * secondSign,
			matrix.m[3][firstColumn] + matrix.m[3][secondColumn] * secondSign);
	}
};
} // namespace KCE
