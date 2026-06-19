#pragma once
#include <cmath>

/**
 * @brief 2次元ベクトル構造体
 * 
 * 2D空間における位置、方向、速度などを表現する。
 * 基本的なベクトル演算（加算、減算、スカラー乗算、正規化、内積など）をサポート。
 */
struct Vector2 {
    float x, y; // X成分, Y成分

    /**
     * @brief ベクトルを正規化（長さを1にする）
     * @return 正規化されたベクトル（長さ0の場合はゼロベクトル）
     */
    Vector2 Normalize() const {
        float len = Length();
        if (len == 0) return Vector2{ 0.0f, 0.0f };
        return *this / len;
    }

    /**
     * @brief ベクトルの長さを取得
     * @return ベクトルの長さ（ユークリッドノルム）
     */
    float Length() const {
        return sqrtf(x * x + y * y);
    }

    /**
     * @brief 内積を計算
     * @param other 内積を計算する相手のベクトル
     * @return 内積の値
     */
    float Dot(const Vector2& other) const {
        return (x * other.x) + (y * other.y);
    }

    /**
     * @brief ベクトルを正規化（静的関数版）
     * @param vec 正規化するベクトル
     * @return 正規化されたベクトル
     */
    static Vector2 Normalize(const Vector2& vec) {
        float len = vec.Length();
        if (len == 0) return Vector2{ 0.0f, 0.0f };
        return vec / len;
    }

    /**
     * @brief ベクトルの長さを取得（静的関数版）
     * @param vec 長さを計算するベクトル
     * @return ベクトルの長さ
     */
    static float Length(const Vector2& vec) {
        return sqrtf(vec.x * vec.x + vec.y * vec.y);
    }

    /**
     * @brief 内積を計算（静的関数版）
     * @param a ベクトル1
     * @param b ベクトル2
     * @return 内積の値
     */
    static float Dot(const Vector2& a, const Vector2& b) {
        return (a.x * b.x) + (a.y * b.y);
    }

    // --- 演算子オーバーロード ---

    /** @brief ベクトルの加算 */
    Vector2 operator+(const Vector2& other) const {
        return Vector2{ x + other.x, y + other.y };
    }

    /** @brief ベクトルの減算 */
    Vector2 operator-(const Vector2& other) const {
        return Vector2{ x - other.x, y - other.y };
    }

    /** @brief スカラー乗算 */
    Vector2 operator*(float scalar) const {
        return Vector2{ x * scalar, y * scalar };
    }

    /** @brief スカラー除算 */
    Vector2 operator/(float scalar) const {
        return Vector2{ x / scalar, y / scalar };
    }

    /** @brief 加算代入 */
    Vector2& operator+=(const Vector2& other) {
        x += other.x;
        y += other.y;
        return *this;
    }

    /** @brief 減算代入 */
    Vector2& operator-=(const Vector2& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    /** @brief 等価比較 */
	bool operator==(const Vector2& other) const
	{
		return x == other.x && y == other.y;
	}
};
