#pragma once
#include <cmath>

struct Vector4 {
    float x, y, z, w;

    Vector4 Normalize() const {
        float len = Length();
        if (len == 0) return Vector4{ 0.0f, 0.0f, 0.0f, 0.0f };
        return *this / len;
    }

    float Length() const {
        return sqrtf(x * x + y * y + z * z + w * w);
    }

    float Dot(const Vector4& other) const {
        return (x * other.x) + (y * other.y) + (z * other.z) + (w * other.w);
    }

    static Vector4 Normalize(const Vector4& vec) {
        float len = vec.Length();
        if (len == 0) return Vector4{ 0.0f, 0.0f, 0.0f, 0.0f };
        return vec / len;
    }

    static float Length(const Vector4& vec) {
        return sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z + vec.w * vec.w);
    }

    static float Dot(const Vector4& a, const Vector4& b) {
        return (a.x * b.x) + (a.y * b.y) + (a.z * b.z) + (a.w * b.w);
    }

    static Vector4 Lerp(const Vector4& start, const Vector4& end, float t) {
        return {
            start.x + (end.x - start.x) * t,
            start.y + (end.y - start.y) * t,
            start.z + (end.z - start.z) * t,
            start.w + (end.w - start.w) * t
        };
    }

    Vector4 operator+(const Vector4& other) const {
        return Vector4{ x + other.x, y + other.y, z + other.z, w + other.w };
    }

    Vector4 operator-(const Vector4& other) const {
        return Vector4{ x - other.x, y - other.y, z - other.z, w - other.w };
    }

    Vector4 operator*(float scalar) const {
        return Vector4{ x * scalar, y * scalar, z * scalar, w * scalar };
    }

    Vector4 operator/(float scalar) const {
        return Vector4{ x / scalar, y / scalar, z / scalar, w / scalar };
    }

    Vector4& operator+=(const Vector4& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        w += other.w;
        return *this;
    }

    Vector4& operator-=(const Vector4& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        w -= other.w;
        return *this;
    }

    Vector4& operator*=(float scalar) {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        w *= scalar;
        return *this;
    }

    Vector4& operator/=(float scalar) {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        w /= scalar;
        return *this;
    }
};
