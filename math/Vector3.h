#pragma once
#include <cmath>
#include <iostream>

namespace KCE
{
/// IsZero判定用のイプシロンの2乗
constexpr float kEpsilonSquared = 1e-12f;

/**
 * @brief 3次元ベクトル構造体
 * 
 * 3D空間における位置、方向、速度などを表現する。
 * 基本的なベクトル演算（加算、減算、スカラー乗算、正規化、内積、外積など）をサポート。
 */
struct Vector3
{
	float x, y, z; // X成分, Y成分, Z成分

	// --- 基本処理 ---

	/**
	 * @brief ベクトルの長さを取得
	 * @return ベクトルの長さ（ユークリッドノルム）
	 */
	float Length() const
	{
		return std::sqrt(x * x + y * y + z * z);
	}

	/**
	 * @brief ベクトルの長さの2乗を取得（平方根計算を省略）
	 * @return ベクトルの長さの2乗
	 */
	float LengthSquared() const
	{
		return x * x + y * y + z * z;
	}

	/**
	 * @brief ベクトルを正規化（長さを1にする）
	 * @return 正規化されたベクトル（長さ0の場合はゼロベクトル）
	 */
	Vector3 Normalize() const
	{
		float len = Length();
		if (len == 0.0f) return Vector3{ 0.0f, 0.0f, 0.0f };
		return *this / len;
	}

	/**
	 * @brief 自身を正規化（破壊的操作）
	 */
	void NormalizeSelf()
	{
		float len = Length();
		if (len == 0.0f) return;
		x /= len;
		y /= len;
		z /= len;
	}

	/**
	 * @brief ベクトルがゼロに近いかどうかを判定
	 * @param epsilon 判定閾値
	 * @return ゼロに近い場合はtrue
	 */
	bool IsZero(float epsilon = 1e-6f) const
	{
		return LengthSquared() < epsilon * epsilon;
	}

	// --- 静的関数 ---

	/**
	 * @brief ベクトルを正規化（静的関数版）
	 * @param vec 正規化するベクトル
	 * @return 正規化されたベクトル
	 */
	static Vector3 Normalize(const Vector3& vec)
	{
		return vec.Normalize();
	}

	/**
	 * @brief ベクトルの長さを取得（静的関数版）
	 * @param vec 長さを計算するベクトル
	 * @return ベクトルの長さ
	 */
	static float Length(const Vector3& vec)
	{
		return vec.Length();
	}

	/**
	 * @brief ベクトルの長さの2乗を取得（静的関数版）
	 * @param vec 計算対象のベクトル
	 * @return ベクトルの長さの2乗
	 */
	static float LengthSquared(const Vector3& vec)
	{
		return vec.LengthSquared();
	}

	/**
	 * @brief 内積を計算（a・b = |a||b|cos(θ)）
	 * @param a ベクトル1
	 * @param b ベクトル2
	 * @return 内積の値
	 */
	static float Dot(const Vector3& a, const Vector3& b)
	{
		return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
	}

	/**
	 * @brief 外積を計算（a × b、aとbに垂直なベクトル）
	 * @param a ベクトル1
	 * @param b ベクトル2
	 * @return 外積ベクトル
	 */
	static Vector3 Cross(const Vector3& a, const Vector3& b)
	{
		return Vector3{
			(a.y * b.z) - (a.z * b.y),
			(a.z * b.x) - (a.x * b.z),
			(a.x * b.y) - (a.y * b.x)
		};
	}

	/**
	 * @brief 2点間の距離を計算
	 * @param a 点1
	 * @param b 点2
	 * @return 2点間の距離
	 */
	static float Distance(const Vector3& a, const Vector3& b)
	{
		return (a - b).Length();
	}

	/**
	 * @brief 2点間の距離の2乗を計算
	 * @param a 点1
	 * @param b 点2
	 * @return 2点間の距離の2乗
	 */
	static float DistanceSquared(const Vector3& a, const Vector3& b)
	{
		return (a - b).LengthSquared();
	}

	/**
	 * @brief 軸周りにベクトルを回転（ロドリゲスの回転公式）
	 * @param vec 回転するベクトル
	 * @param axis 回転軸（正規化済みである必要はない）
	 * @param angleRad 回転角度（ラジアン）
	 * @return 回転後のベクトル
	 */
	static Vector3 Rotate(const Vector3& vec, const Vector3& axis, float angleRad)
	{
		Vector3 normAxis = Normalize(axis);
		float cosTheta = std::cos(angleRad);
		float sinTheta = std::sin(angleRad);

		return vec * cosTheta +
			Cross(normAxis, vec) * sinTheta +
			normAxis * Dot(normAxis, vec) * (1 - cosTheta);
	}

	// --- 演算子オーバーロード ---

	/** @brief ベクトルの加算 */
	Vector3 operator+(const Vector3& other) const
	{
		return Vector3{ x + other.x, y + other.y, z + other.z };
	}

	/** @brief ベクトルの減算 */
	Vector3 operator-(const Vector3& other) const
	{
		return Vector3{ x - other.x, y - other.y, z - other.z };
	}

	/** @brief 成分ごとの乗算 */
	Vector3 operator*(const Vector3& other) const
	{
		return Vector3{ x * other.x, y * other.y, z * other.z };
	}

	/** @brief スカラー乗算 */
	Vector3 operator*(float scalar) const
	{
		return Vector3{ x * scalar, y * scalar, z * scalar };
	}

	/** @brief スカラー除算 */
	Vector3 operator/(float scalar) const
	{
		if (scalar == 0.0f) return Vector3{ 0.0f, 0.0f, 0.0f };
		return Vector3{ x / scalar, y / scalar, z / scalar };
	}

	/** @brief 加算代入 */
	Vector3& operator+=(const Vector3& other)
	{
		x += other.x;
		y += other.y;
		z += other.z;
		return *this;
	}

	/** @brief 減算代入 */
	Vector3& operator-=(const Vector3& other)
	{
		x -= other.x;
		y -= other.y;
		z -= other.z;
		return *this;
	}

	/** @brief スカラー乗算代入 */
	Vector3& operator*=(float scalar)
	{
		x *= scalar;
		y *= scalar;
		z *= scalar;
		return *this;
	}

	/** @brief スカラー除算代入 */
	Vector3& operator/=(float scalar)
	{
		if (scalar != 0.0f)
		{
			x /= scalar;
			y /= scalar;
			z /= scalar;
		}
		return *this;
	}

	/** @brief 等価比較 */
	bool operator==(const Vector3& other) const
	{
		return x == other.x && y == other.y && z == other.z;
	}

	/** @brief 非等価比較 */
	bool operator!=(const Vector3& other) const
	{
		return !(*this == other);
	}

	/** @brief 単項マイナス（反転） */
	Vector3 operator-() const
	{
		return Vector3{ -x, -y, -z };
	}

	/** @brief 単項プラス */
	Vector3 operator+() const
	{
		return *this;
	}

	/** @brief 成分ごとの乗算代入 */
	Vector3& operator*=(const Vector3& other)
	{
		x *= other.x;
		y *= other.y;
		z *= other.z;
		return *this;
	}

	/** @brief スカラー × ベクトル（フレンド関数） */
	friend Vector3 operator*(float scalar, const Vector3& vec)
	{
		return vec * scalar;
	}
	
	// --- 最小・最大 ---

	/**
	 * @brief 各成分の最小値を取得
	 * @param a ベクトル1
	 * @param b ベクトル2
	 * @return 各成分が最小値のベクトル
	 */
	static Vector3 Min(const Vector3& a, const Vector3& b)
	{
		return Vector3{ std::fmin(a.x, b.x), std::fmin(a.y, b.y), std::fmin(a.z, b.z) };
	}

	/**
	 * @brief 各成分の最大値を取得
	 * @param a ベクトル1
	 * @param b ベクトル2
	 * @return 各成分が最大値のベクトル
	 */
	static Vector3 Max(const Vector3& a, const Vector3& b)
	{
		return Vector3{ std::fmax(a.x, b.x), std::fmax(a.y, b.y), std::fmax(a.z, b.z) };
	}

};
} // namespace KCE
