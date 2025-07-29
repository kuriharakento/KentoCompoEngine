#pragma once
#include <cmath>
#include <iostream>

struct Vector3
{
	float x, y, z;

	// --- 基本処理 ---
	float Length() const
	{
		return std::sqrt(x * x + y * y + z * z);
	}

	float LengthSquared() const
	{
		return x * x + y * y + z * z;
	}

	Vector3 Normalize() const
	{
		float len = Length();
		if (len == 0.0f) return Vector3{ 0.0f, 0.0f, 0.0f };
		return *this / len;
	}

	bool IsZero(float epsilon = 1e-6f) const
	{
		return LengthSquared() < epsilon * epsilon;
	}

	// --- 静的関数 ---
	static Vector3 Normalize(const Vector3& vec)
	{
		return vec.Normalize();
	}

	static float Length(const Vector3& vec)
	{
		return vec.Length();
	}

	static float LengthSquared(const Vector3& vec)
	{
		return vec.LengthSquared();
	}

	static float Dot(const Vector3& a, const Vector3& b)
	{
		return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
	}

	static Vector3 Cross(const Vector3& a, const Vector3& b)
	{
		return Vector3{
			(a.y * b.z) - (a.z * b.y),
			(a.z * b.x) - (a.x * b.z),
			(a.x * b.y) - (a.y * b.x)
		};
	}

	static float Distance(const Vector3& a, const Vector3& b)
	{
		return (a - b).Length();
	}

	static float DistanceSquared(const Vector3& a, const Vector3& b)
	{
		return (a - b).LengthSquared();
	}

	static Vector3 Rotate(const Vector3& vec, const Vector3& axis, float angleRad)
	{
		Vector3 normAxis = Normalize(axis);
		float cosTheta = std::cos(angleRad);
		float sinTheta = std::sin(angleRad);

		return vec * cosTheta +
			Cross(normAxis, vec) * sinTheta +
			normAxis * Dot(normAxis, vec) * (1 - cosTheta);
	}

	// --- 演算子 ---
	Vector3 operator+(const Vector3& other) const
	{
		return Vector3{ x + other.x, y + other.y, z + other.z };
	}

	Vector3 operator-(const Vector3& other) const
	{
		return Vector3{ x - other.x, y - other.y, z - other.z };
	}

	Vector3 operator*(const Vector3& other) const
	{
		return Vector3{ x * other.x, y * other.y, z * other.z };
	}

	Vector3 operator*(float scalar) const
	{
		return Vector3{ x * scalar, y * scalar, z * scalar };
	}

	Vector3 operator/(float scalar) const
	{
		if (scalar == 0.0f) return Vector3{ 0.0f, 0.0f, 0.0f };
		return Vector3{ x / scalar, y / scalar, z / scalar };
	}

	Vector3& operator+=(const Vector3& other)
	{
		x += other.x;
		y += other.y;
		z += other.z;
		return *this;
	}

	Vector3& operator-=(const Vector3& other)
	{
		x -= other.x;
		y -= other.y;
		z -= other.z;
		return *this;
	}

	Vector3& operator*=(float scalar)
	{
		x *= scalar;
		y *= scalar;
		z *= scalar;
		return *this;
	}

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

	bool operator==(const Vector3& other) const
	{
		return x == other.x && y == other.y && z == other.z;
	}

	bool operator!=(const Vector3& other) const
	{
		return !(*this == other);
	}

	Vector3 operator-() const
	{
		return Vector3{ -x, -y, -z };
	}

	Vector3 operator+() const
	{
		return *this;
	}
	Vector3& operator*=(const Vector3& other)
	{
		x *= other.x;
		y *= other.y;
		z *= other.z;
		return *this;
	}
	// --- フレンド関数（スカラー × ベクトル） ---
	friend Vector3 operator*(float scalar, const Vector3& vec)
	{
		return vec * scalar;
	}
	
	// --- 最小・最大 ---
	static Vector3 Min(const Vector3& a, const Vector3& b)
	{
		return Vector3{ std::fmin(a.x, b.x), std::fmin(a.y, b.y), std::fmin(a.z, b.z) };
	}
	static Vector3 Max(const Vector3& a, const Vector3& b)
	{
		return Vector3{ std::fmax(a.x, b.x), std::fmax(a.y, b.y), std::fmax(a.z, b.z) };
	}

};
