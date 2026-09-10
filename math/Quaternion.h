#pragma once
#include <cmath>
#include "Vector3.h"
#include "MatrixFunc.h"

namespace KCE
{
/**
 * @brief クォータニオン
 * @details 回転を表現するための四元数。Slerp補間に対応。
 */
struct Quaternion
{
    float x, y, z, w;

    /**
     * @brief デフォルトコンストラクタ（単位クォータニオン）
     */
    Quaternion() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}

    /**
     * @brief パラメータ付きコンストラクタ
     */
    Quaternion(float x_, float y_, float z_, float w_)
        : x(x_), y(y_), z(z_), w(w_) {}

    /**
     * @brief 単位クォータニオンを返す
     */
    static Quaternion Identity()
    {
        return Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
    }

    /**
     * @brief オイラー角からクォータニオンを生成
     * @param euler オイラー角（ラジアン）
     */
    static Quaternion FromEuler(const Vector3& euler)
    {
        float cx = std::cos(euler.x * 0.5f);
        float sx = std::sin(euler.x * 0.5f);
        float cy = std::cos(euler.y * 0.5f);
        float sy = std::sin(euler.y * 0.5f);
        float cz = std::cos(euler.z * 0.5f);
        float sz = std::sin(euler.z * 0.5f);

        return Quaternion(
            sx * cy * cz - cx * sy * sz,
            cx * sy * cz + sx * cy * sz,
            cx * cy * sz - sx * sy * cz,
            cx * cy * cz + sx * sy * sz
        );
    }

    /**
     * @brief 軸と角度からクォータニオンを生成
     * @param axis 回転軸（正規化済み）
     * @param angle 回転角度（ラジアン）
     */
    static Quaternion FromAxisAngle(const Vector3& axis, float angle)
    {
        float halfAngle = angle * 0.5f;
        float s = std::sin(halfAngle);
        return Quaternion(
            axis.x * s,
            axis.y * s,
            axis.z * s,
            std::cos(halfAngle)
        );
    }

    /**
     * @brief クォータニオンの正規化
     */
    Quaternion Normalized() const
    {
        float len = std::sqrt(x * x + y * y + z * z + w * w);
        if (len > 0.0f)
        {
            float invLen = 1.0f / len;
            return Quaternion(x * invLen, y * invLen, z * invLen, w * invLen);
        }
        return Identity();
    }

    /**
     * @brief クォータニオンの共役
     */
    Quaternion Conjugate() const
    {
        return Quaternion(-x, -y, -z, w);
    }

    /**
     * @brief 回転行列からクォータニオンを復元する
     *
     * @details ToMatrix() の逆変換。ギズモが返してくるワールド行列から
     *          回転だけを取り出すために使う。
     *          引数の回転部は正規直交であること（スケールを含まないこと）。
     * @param matrix 回転行列
     * @return 復元されたクォータニオン
     */
    static Quaternion FromMatrix(const Matrix4x4& matrix)
    {
        // 数値誤差に強いよう、対角成分の中で最大のものを基準に解く
        const float trace = matrix.m[0][0] + matrix.m[1][1] + matrix.m[2][2];

        if (trace > 0.0f)
        {
            const float s = std::sqrt(trace + 1.0f) * 2.0f;
            return Quaternion(
                (matrix.m[1][2] - matrix.m[2][1]) / s,
                (matrix.m[2][0] - matrix.m[0][2]) / s,
                (matrix.m[0][1] - matrix.m[1][0]) / s,
                0.25f * s
            ).Normalized();
        }

        if (matrix.m[0][0] > matrix.m[1][1] && matrix.m[0][0] > matrix.m[2][2])
        {
            const float s = std::sqrt(1.0f + matrix.m[0][0] - matrix.m[1][1] - matrix.m[2][2]) * 2.0f;
            return Quaternion(
                0.25f * s,
                (matrix.m[0][1] + matrix.m[1][0]) / s,
                (matrix.m[2][0] + matrix.m[0][2]) / s,
                (matrix.m[1][2] - matrix.m[2][1]) / s
            ).Normalized();
        }

        if (matrix.m[1][1] > matrix.m[2][2])
        {
            const float s = std::sqrt(1.0f + matrix.m[1][1] - matrix.m[0][0] - matrix.m[2][2]) * 2.0f;
            return Quaternion(
                (matrix.m[0][1] + matrix.m[1][0]) / s,
                0.25f * s,
                (matrix.m[1][2] + matrix.m[2][1]) / s,
                (matrix.m[2][0] - matrix.m[0][2]) / s
            ).Normalized();
        }

        const float s = std::sqrt(1.0f + matrix.m[2][2] - matrix.m[0][0] - matrix.m[1][1]) * 2.0f;
        return Quaternion(
            (matrix.m[2][0] + matrix.m[0][2]) / s,
            (matrix.m[1][2] + matrix.m[2][1]) / s,
            0.25f * s,
            (matrix.m[0][1] - matrix.m[1][0]) / s
        ).Normalized();
    }

    /**
     * @brief オイラー角に変換（FromEulerの逆変換）
     *
     * @details FromEuler と同じ XYZ 順の規約で復元する。
     *          エディタでの数値表示・入力のためのものであり、
     *          補間には使わないこと（オイラー角のまま補間するとジンバルロックで破綻する）。
     *          真上・真下を向いた特異点では Z 回転に縮退する。
     * @return オイラー角（ラジアン）
     */
    Vector3 ToEuler() const
    {
        // 行ベクトル規約で R = Rx * Ry * Rz を展開すると
        //   m[0][2] = -sin(y),  m[1][2] = sin(x)cos(y),  m[2][2] = cos(x)cos(y)
        //   m[0][0] = cos(y)cos(z),  m[0][1] = cos(y)sin(z)
        // となる。これを ToMatrix() の各成分に置き換えて解く。
        Vector3 euler{};

        float sinY = 2.0f * (w * y - x * z);
        sinY = sinY < -1.0f ? -1.0f : (sinY > 1.0f ? 1.0f : sinY);

        if (std::abs(sinY) >= 0.99999f)
        {
            // 特異点（Y が ±90 度）。cos(y) が 0 になり X と Z が区別できなくなるので、
            // Z を 0 に固定して X に回転をまとめる。
            euler.x = std::atan2(2.0f * (w * x - y * z), 1.0f - 2.0f * (x * x + z * z));
            euler.y = std::copysign(3.14159265358979323846f * 0.5f, sinY);
            euler.z = 0.0f;
            return euler;
        }

        euler.x = std::atan2(2.0f * (y * z + w * x), 1.0f - 2.0f * (x * x + y * y));
        euler.y = std::asin(sinY);
        euler.z = std::atan2(2.0f * (x * y + w * z), 1.0f - 2.0f * (y * y + z * z));
        return euler;
    }

    /**
     * @brief 球面線形補間（Slerp）
     * @param a 開始クォータニオン
     * @param b 終了クォータニオン
     * @param t 補間係数（0.0〜1.0）
     */
    static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t)
    {
        // 内積を計算
        float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;

        // 短い経路を選択（内積が負なら片方を反転）
        Quaternion end = b;
        if (dot < 0.0f)
        {
            dot = -dot;
            end = Quaternion(-b.x, -b.y, -b.z, -b.w);
        }

        // ほぼ同じ向きなら線形補間
        if (dot > 0.9995f)
        {
            return Quaternion(
                a.x + t * (end.x - a.x),
                a.y + t * (end.y - a.y),
                a.z + t * (end.z - a.z),
                a.w + t * (end.w - a.w)
            ).Normalized();
        }

        // Slerp
        float theta0 = std::acos(dot);
        float theta = theta0 * t;
        float sinTheta = std::sin(theta);
        float sinTheta0 = std::sin(theta0);

        float s0 = std::cos(theta) - dot * sinTheta / sinTheta0;
        float s1 = sinTheta / sinTheta0;

        return Quaternion(
            s0 * a.x + s1 * end.x,
            s0 * a.y + s1 * end.y,
            s0 * a.z + s1 * end.z,
            s0 * a.w + s1 * end.w
        ).Normalized();
    }

    /**
     * @brief クォータニオンを回転行列に変換
     */
    Matrix4x4 ToMatrix() const
    {
        Matrix4x4 result;

        float xx = x * x;
        float yy = y * y;
        float zz = z * z;
        float xy = x * y;
        float xz = x * z;
        float yz = y * z;
        float wx = w * x;
        float wy = w * y;
        float wz = w * z;

        result.m[0][0] = 1.0f - 2.0f * (yy + zz);
        result.m[0][1] = 2.0f * (xy + wz);
        result.m[0][2] = 2.0f * (xz - wy);
        result.m[0][3] = 0.0f;

        result.m[1][0] = 2.0f * (xy - wz);
        result.m[1][1] = 1.0f - 2.0f * (xx + zz);
        result.m[1][2] = 2.0f * (yz + wx);
        result.m[1][3] = 0.0f;

        result.m[2][0] = 2.0f * (xz + wy);
        result.m[2][1] = 2.0f * (yz - wx);
        result.m[2][2] = 1.0f - 2.0f * (xx + yy);
        result.m[2][3] = 0.0f;

        result.m[3][0] = 0.0f;
        result.m[3][1] = 0.0f;
        result.m[3][2] = 0.0f;
        result.m[3][3] = 1.0f;

        return result;
    }

    /**
     * @brief クォータニオン同士の乗算
     */
    Quaternion operator*(const Quaternion& other) const
    {
        return Quaternion(
            w * other.x + x * other.w + y * other.z - z * other.y,
            w * other.y - x * other.z + y * other.w + z * other.x,
            w * other.z + x * other.y - y * other.x + z * other.w,
            w * other.w - x * other.x - y * other.y - z * other.z
        );
    }

    /**
     * @brief ベクトルの回転
     */
    Vector3 RotateVector(const Vector3& v) const
    {
        Quaternion p(v.x, v.y, v.z, 0.0f);
        Quaternion result = *this * p * Conjugate();
        return Vector3(result.x, result.y, result.z);
    }
};
} // namespace KCE
