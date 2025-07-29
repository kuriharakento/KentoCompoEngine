#include "MathUtils.h"

#include <cassert>
#include <random>

namespace MathUtils
{

	float RandomFloat(float min, float max)
	{
		static std::random_device rd;  // シード生成器（staticで初期化は1回だけ）
		static std::mt19937 gen(rd()); // メルセンヌ・ツイスタの乱数生成器

		std::uniform_real_distribution<float> dist(min, max);
		return dist(gen);
	}

	Vector3 RandomVector3(Vector3 min, Vector3 max)
	{
		return Vector3(RandomFloat(min.x, max.x), RandomFloat(min.y, max.y), RandomFloat(min.z, max.z));
	}

	Vector4 RandomVector4(Vector4 min, Vector4 max)
	{
		return Vector4(RandomFloat(min.x, max.x), RandomFloat(min.y, max.y), RandomFloat(min.z, max.z), RandomFloat(min.w, max.w));
	}

	Vector3 GetMatrixTranslate(const Matrix4x4& matrix)
	{
		return Vector3(matrix.m[3][0], matrix.m[3][1], matrix.m[3][2]);
	}

	Vector3 GetMatrixScale(const Matrix4x4& matrix)
	{
		return Vector3(
			std::sqrt(matrix.m[0][0] * matrix.m[0][0] + matrix.m[1][0] * matrix.m[1][0] + matrix.m[2][0] * matrix.m[2][0]),
			std::sqrt(matrix.m[0][1] * matrix.m[0][1] + matrix.m[1][1] * matrix.m[1][1] + matrix.m[2][1] * matrix.m[2][1]),
			std::sqrt(matrix.m[0][2] * matrix.m[0][2] + matrix.m[1][2] * matrix.m[1][2] + matrix.m[2][2] * matrix.m[2][2])
		);
	}

	Vector3 GetMatrixRotate(const Matrix4x4& matrix)
	{
		Vector3 rotate;
		rotate.y = std::asin(-matrix.m[0][2]);

		if (std::cos(rotate.y) != 0.0f)
		{
			rotate.x = std::atan2(matrix.m[1][2], matrix.m[2][2]);
			rotate.z = std::atan2(matrix.m[0][1], matrix.m[0][0]);
		}
		else
		{
			rotate.x = std::atan2(-matrix.m[2][0], matrix.m[1][0]);
			rotate.z = 0.0f;
		}

		return rotate;
	}

	float Clamp(float value, float min, float max)
	{
		if (value < min) return min;
		if (value > max) return max;
		return value;
	}

	Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix)
	{
		Vector3 result;
		result.x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0] + matrix.m[3][0];
		result.y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1] + matrix.m[3][1];
		result.z = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2] + matrix.m[3][2];
		float w = vector.x * matrix.m[0][3] + vector.y * matrix.m[1][3] + vector.z * matrix.m[2][3] + matrix.m[3][3];
		assert(w != 0.0f);
		result.x /= w;
		result.y /= w;
		result.z /= w;
		return result;
	}

	Vector3 TransformNormal(const Vector3& normal, const Matrix4x4& matrix)
	{
		Vector3 result;
		result.x = normal.x * matrix.m[0][0] + normal.y * matrix.m[1][0] + normal.z * matrix.m[2][0];
		result.y = normal.x * matrix.m[0][1] + normal.y * matrix.m[1][1] + normal.z * matrix.m[2][1];
		result.z = normal.x * matrix.m[0][2] + normal.y * matrix.m[1][2] + normal.z * matrix.m[2][2];
		return result;
	}

	Vector3 CalculateOrbitPosition(const Vector3& center, float radius, float angle)
	{
		return center + Vector3(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
	}

	Vector3 CalculateYawPitchFromDirection(const Vector3& direction)
	{
		if (direction.LengthSquared() == 0.0f)
		{
			//回転不要
			return Vector3(0.0f, 0.0f, 0.0f);
		}

		direction.Normalize();

		float yaw = std::atan2(direction.x, direction.z);
		float pitch = std::atan2(direction.y, std::sqrt(direction.x * direction.x + direction.z * direction.z));
		return Vector3(-pitch, yaw, 0.0f);
	}

	Vector3 CalculateDirectionToTarget(const Vector3& currentPosition, const Vector3& targetPosition)
	{
		// ターゲット方向のベクトルを計算
		Vector3 direction = targetPosition - currentPosition;

		// ベクトルの長さが0の場合、回転不要
		if (direction.IsZero())
		{
			return Vector3(0.0f, 0.0f, 0.0f);
		}

		// ベクトルを正規化
		direction = direction.Normalize();

		// Yaw（左右の回転角度）を計算
		float yaw = std::atan2(direction.x, direction.z);

		// Pitch（上下の回転角度）を計算
		float horizontalDistance = std::sqrt(direction.x * direction.x + direction.z * direction.z);
		float pitch = std::atan2(direction.y, horizontalDistance);

		// Z軸回転（ロール）は不要なので0
		return Vector3(-pitch, yaw, 0.0f);
	}

	Matrix4x4 Transpose(const Matrix4x4& m)
	{
		Matrix4x4 result;
		for (int i = 0; i < 4; ++i)
		{
			for (int j = 0; j < 4; ++j)
			{
				result.m[i][j] = m.m[j][i];
			}
		}
		return result;
	}



}
