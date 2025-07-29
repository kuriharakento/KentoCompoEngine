#pragma once
#include <cmath>

struct Vector2 {
    float x, y;

    Vector2 Normalize() const {
        float len = Length();
        if (len == 0) return Vector2{ 0.0f, 0.0f };
        return *this / len;
    }

    float Length() const {
        return sqrtf(x * x + y * y);
    }

    float Dot(const Vector2& other) const {
        return (x * other.x) + (y * other.y);
    }

    static Vector2 Normalize(const Vector2& vec) {
        float len = vec.Length();
        if (len == 0) return Vector2{ 0.0f, 0.0f };
        return vec / len;
    }

    static float Length(const Vector2& vec) {
        return sqrtf(vec.x * vec.x + vec.y * vec.y);
    }

    static float Dot(const Vector2& a, const Vector2& b) {
        return (a.x * b.x) + (a.y * b.y);
    }

    Vector2 operator+(const Vector2& other) const {
        return Vector2{ x + other.x, y + other.y };
    }

    Vector2 operator-(const Vector2& other) const {
        return Vector2{ x - other.x, y - other.y };
    }

    Vector2 operator*(float scalar) const {
        return Vector2{ x * scalar, y * scalar };
    }

    Vector2 operator/(float scalar) const {
        return Vector2{ x / scalar, y / scalar };
    }

    Vector2& operator+=(const Vector2& other) {
        x += other.x;
        y += other.y;
        return *this;
    }

    Vector2& operator-=(const Vector2& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }
};
