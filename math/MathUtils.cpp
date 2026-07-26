#include "MathUtils.h"

#include <cassert>
#include <random>

namespace KCE
{
namespace MathUtils
{
	// ゼロ判定の閾値
	constexpr float kZeroThreshold = 1e-6f;
	// 4x4行列のサイズ
	constexpr int kMatrixSize = 4;

	float RandomFloat(float min, float max)
	{
		// min/maxを正しい順序に修正
		if (min > max) std::swap(min, max);

		// シード生成器（staticで初期化は1回だけ）
		static std::random_device rd;
		// メルセンヌ・ツイスタの乱数生成器
		static std::mt19937 gen(rd());

		// 一様分布で乱数を生成
		std::uniform_real_distribution<float> dist(min, max);
		return dist(gen);
	}

	Vector3 RandomVector3(const Vector3& min, const Vector3& max)
	{
		Vector3 vMin = min;
		Vector3 vMax = max;
		// 順序がおかしい場合は正しい順序に修正
		if (vMin.x > vMax.x) std::swap(vMin.x, vMax.x);
		if (vMin.y > vMax.y) std::swap(vMin.y, vMax.y);
		if (vMin.z > vMax.z) std::swap(vMin.z, vMax.z);
		// 各成分に対してランダム値を生成
		return Vector3(RandomFloat(vMin.x, vMax.x), RandomFloat(vMin.y, vMax.y), RandomFloat(vMin.z, vMax.z));
	}

	Vector4 RandomVector4(const Vector4& min, const Vector4& max)
	{
		Vector4 vMin = min;
		Vector4 vMax = max;
		// 順序がおかしい場合は正しい順序に修正
		if (vMin.x > vMax.x) std::swap(vMin.x, vMax.x);
		if (vMin.y > vMax.y) std::swap(vMin.y, vMax.y);
		if (vMin.z > vMax.z) std::swap(vMin.z, vMax.z);
		if (vMin.w > vMax.w) std::swap(vMin.w, vMax.w);
		// 各成分に対してランダム値を生成
		return Vector4(RandomFloat(vMin.x, vMax.x), RandomFloat(vMin.y, vMax.y), RandomFloat(vMin.z, vMax.z), RandomFloat(vMin.w, vMax.w));
	}

	Vector3 GetTranslateFromMatrix(const Matrix4x4& matrix)
	{
		// 行列の第4行から平行移動成分を抽出
		return Vector3(matrix.m[3][0], matrix.m[3][1], matrix.m[3][2]);
	}

	Vector3 GetScaleFromMatrix(const Matrix4x4& matrix)
	{
		// 各軸ベクトルの長さからスケール成分を計算
		return Vector3(
			std::sqrt(matrix.m[0][0] * matrix.m[0][0] + matrix.m[0][1] * matrix.m[0][1] + matrix.m[0][2] * matrix.m[0][2]),
			std::sqrt(matrix.m[1][0] * matrix.m[1][0] + matrix.m[1][1] * matrix.m[1][1] + matrix.m[1][2] * matrix.m[1][2]),
			std::sqrt(matrix.m[2][0] * matrix.m[2][0] + matrix.m[2][1] * matrix.m[2][1] + matrix.m[2][2] * matrix.m[2][2])
		);
	}

	Vector3 GetRotateFromMatrix(const Matrix4x4& matrix)
	{
		// スケール成分を取得
		Vector3 scale = GetScaleFromMatrix(matrix);

		// スケールが0に近い場合はゼロ回転を返す
		if (scale.x < kZeroThreshold || scale.y < kZeroThreshold || scale.z < kZeroThreshold)
		{
			return Vector3(0.0f, 0.0f, 0.0f);
		}

		// スケールで正規化された回転成分を計算
		float rm00 = matrix.m[0][0] / scale.x;
		float rm01 = matrix.m[0][1] / scale.x;
		float rm02 = matrix.m[0][2] / scale.x;
		float rm10 = matrix.m[1][0] / scale.y;
		float rm11 = matrix.m[1][1] / scale.y;
		float rm12 = matrix.m[1][2] / scale.y;
		float rm20 = matrix.m[2][0] / scale.z;
		float rm21 = matrix.m[2][1] / scale.z;
		float rm22 = matrix.m[2][2] / scale.z;

		Vector3 rotate;
		// Y軸回転を計算（ジンバルロック対策でクランプ）
		rotate.y = std::asin(Clamp(-rm02, -1.0f, 1.0f));

		const float cosY = std::cos(rotate.y);
		// cosYが十分大きい場合は通常の計算
		if (cosY > kZeroThreshold)
		{
			rotate.x = std::atan2(rm12, rm22);
			rotate.z = std::atan2(rm01, rm00);
		}
		else
		{
			// ジンバルロック時の特殊処理
			rotate.x = std::atan2(-rm21, rm11);
			rotate.z = 0.0f;
		}

		return rotate;
	}

	Matrix4x4 GetMatrixRotate(const Matrix4x4& matrix)
	{
		// スケール成分を取得
		Vector3 scale = GetScaleFromMatrix(matrix);

		// スケールが0に近い場合は単位行列を返す
		if (scale.x < kZeroThreshold || scale.y < kZeroThreshold || scale.z < kZeroThreshold)
		{
			return MakeIdentity4x4();
		}

		Matrix4x4 rotation = {};
		// 正規化された回転成分を抽出（X軸）
		rotation.m[0][0] = matrix.m[0][0] / scale.x;
		rotation.m[0][1] = matrix.m[0][1] / scale.x;
		rotation.m[0][2] = matrix.m[0][2] / scale.x;

		// 正規化された回転成分を抽出（Y軸）
		rotation.m[1][0] = matrix.m[1][0] / scale.y;
		rotation.m[1][1] = matrix.m[1][1] / scale.y;
		rotation.m[1][2] = matrix.m[1][2] / scale.y;

		// 正規化された回転成分を抽出（Z軸）
		rotation.m[2][0] = matrix.m[2][0] / scale.z;
		rotation.m[2][1] = matrix.m[2][1] / scale.z;
		rotation.m[2][2] = matrix.m[2][2] / scale.z;

		// 同次座標の成分
		rotation.m[3][3] = 1.0f;

		return rotation;
	}

	float Clamp(float value, float min, float max)
	{
		// 下限と上限でクリップ
		if (value < min) return min;
		if (value > max) return max;
		return value;
	}

	Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix)
	{
		Vector3 result;
		// 行列とベクトルの積を計算
		result.x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0] + matrix.m[3][0];
		result.y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1] + matrix.m[3][1];
		result.z = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2] + matrix.m[3][2];
		// 同次座標のw成分を計算
		float w = vector.x * matrix.m[0][3] + vector.y * matrix.m[1][3] + vector.z * matrix.m[2][3] + matrix.m[3][3];
		assert(w != 0.0f);
		// w成分で除算して正規化
		result.x /= w;
		result.y /= w;
		result.z /= w;
		return result;
	}

	Vector3 TransformNormal(const Vector3& normal, const Matrix4x4& matrix)
	{
		Vector3 result;
		// 法線は平行移動成分を無視して変換
		result.x = normal.x * matrix.m[0][0] + normal.y * matrix.m[1][0] + normal.z * matrix.m[2][0];
		result.y = normal.x * matrix.m[0][1] + normal.y * matrix.m[1][1] + normal.z * matrix.m[2][1];
		result.z = normal.x * matrix.m[0][2] + normal.y * matrix.m[1][2] + normal.z * matrix.m[2][2];
		return result;
	}

	Vector3 CalculateOrbitPosition(const Vector3& center, float radius, float angle)
	{
		// XZ平面上の円軌道を計算
		return center + Vector3(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
	}

	Vector3 CalculateYawPitchFromDirection(const Vector3& direction)
	{
		// 長さがゼロの場合は回転不要
		if (direction.LengthSquared() == 0.0f)
		{
			return Vector3(0.0f, 0.0f, 0.0f);
		}

		// 方向ベクトルを正規化
		Vector3 normDirection = direction.Normalize();

		// Yaw（左右回転）を計算
		float yaw = std::atan2(normDirection.x, normDirection.z);
		// Pitch（上下回転）を計算
		float pitch = std::atan2(normDirection.y, std::sqrt(normDirection.x * normDirection.x + normDirection.z * normDirection.z));
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

		// 水平距離を計算
		float horizontalDistance = std::sqrt(direction.x * direction.x + direction.z * direction.z);
		// Pitch（上下の回転角度）を計算
		float pitch = std::atan2(direction.y, horizontalDistance);

		// Z軸回転（ロール）は不要なので0
		return Vector3(-pitch, yaw, 0.0f);
	}

	Matrix4x4 Transpose(const Matrix4x4& m)
	{
		Matrix4x4 result;
		// 行と列を入れ替え
		for (int i = 0; i < kMatrixSize; ++i)
		{
			for (int j = 0; j < kMatrixSize; ++j)
			{
				result.m[i][j] = m.m[j][i];
			}
		}
		return result;
	}



}
} // namespace KCE
