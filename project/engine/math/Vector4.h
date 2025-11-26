#pragma once
#include <cmath>

/**
 * @brief 4次元ベクトル構造体
 * 
 * 4D空間における位置やカラー（RGBA）を表現する。
 * 同次座標系での3D変換にも使用される。
 * 基本的なベクトル演算（加算、減算、スカラー乗算、正規化、内積など）をサポート。
 */
struct Vector4 {
    float x, y, z, w; // X成分, Y成分, Z成分, W成分

    /**
     * @brief ベクトルを正規化（長さを1にする）
     * @return 正規化されたベクトル（長さ0の場合はゼロベクトル）
     */
    Vector4 Normalize() const {
        float len = Length();
        if (len == 0) return Vector4{ 0.0f, 0.0f, 0.0f, 0.0f };
        return *this / len;
    }

    /**
     * @brief ベクトルの長さを取得
     * @return ベクトルの長さ（ユークリッドノルム）
     */
    float Length() const {
        return sqrtf(x * x + y * y + z * z + w * w);
    }

    /**
     * @brief 内積を計算
     * @param other 内積を計算する相手のベクトル
     * @return 内積の値
     */
    float Dot(const Vector4& other) const {
        return (x * other.x) + (y * other.y) + (z * other.z) + (w * other.w);
    }

    /**
     * @brief ベクトルを正規化（静的関数版）
     * @param vec 正規化するベクトル
     * @return 正規化されたベクトル
     */
    static Vector4 Normalize(const Vector4& vec) {
        float len = vec.Length();
        if (len == 0) return Vector4{ 0.0f, 0.0f, 0.0f, 0.0f };
        return vec / len;
    }

    /**
     * @brief ベクトルの長さを取得（静的関数版）
     * @param vec 長さを計算するベクトル
     * @return ベクトルの長さ
     */
    static float Length(const Vector4& vec) {
        return sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z + vec.w * vec.w);
    }

    /**
     * @brief 内積を計算（静的関数版）
     * @param a ベクトル1
     * @param b ベクトル2
     * @return 内積の値
     */
    static float Dot(const Vector4& a, const Vector4& b) {
        return (a.x * b.x) + (a.y * b.y) + (a.z * b.z) + (a.w * b.w);
    }

    /**
     * @brief 線形補間（Lerp）
     * @param start 開始ベクトル
     * @param end 終了ベクトル
     * @param t 補間係数（0.0〜1.0）
     * @return 補間されたベクトル
     */
    static Vector4 Lerp(const Vector4& start, const Vector4& end, float t) {
        return {
            start.x + (end.x - start.x) * t,
            start.y + (end.y - start.y) * t,
            start.z + (end.z - start.z) * t,
            start.w + (end.w - start.w) * t
        };
    }

    // --- 演算子オーバーロード ---

    /** @brief ベクトルの加算 */
    Vector4 operator+(const Vector4& other) const {
        return Vector4{ x + other.x, y + other.y, z + other.z, w + other.w };
    }

    /** @brief ベクトルの減算 */
    Vector4 operator-(const Vector4& other) const {
        return Vector4{ x - other.x, y - other.y, z - other.z, w - other.w };
    }

    /** @brief スカラー乗算 */
    Vector4 operator*(float scalar) const {
        return Vector4{ x * scalar, y * scalar, z * scalar, w * scalar };
    }

    /** @brief スカラー除算 */
    Vector4 operator/(float scalar) const {
        return Vector4{ x / scalar, y / scalar, z / scalar, w / scalar };
    }

    /** @brief 加算代入 */
    Vector4& operator+=(const Vector4& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        w += other.w;
        return *this;
    }

    /** @brief 減算代入 */
    Vector4& operator-=(const Vector4& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        w -= other.w;
        return *this;
    }

    /** @brief スカラー乗算代入 */
    Vector4& operator*=(float scalar) {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        w *= scalar;
        return *this;
    }

    /** @brief スカラー除算代入 */
    Vector4& operator/=(float scalar) {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        w /= scalar;
        return *this;
    }
};
