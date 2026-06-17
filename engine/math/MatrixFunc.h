#pragma once

#include <cmath>

#include "Vector3.h"
#include "Vector4.h"

/**
 * @brief コタンジェント（余接）を計算
 * @param a 角度（ラジアン）
 * @return コタンジェント値 cos(a) / sin(a)
 */
inline float cot(float a) { return cos(a) / sin(a); }

/**
 * @brief 3x3行列構造体
 */
struct Matrix3x3
{
	float m[3][3]; // 3x3の行列要素
};

/**
 * @brief 4x4行列構造体
 * 
 * 3D変換（平行移動、回転、スケール、透視投影など）に使用する行列。
 * 行優先（row-major）形式で格納される。
 */
struct Matrix4x4
{
	float m[4][4]; // 4x4の行列要素

	/**
	 * @brief 4x4行列同士の掛け算
	 * @param other 乗算する行列
	 * @return 乗算結果の行列
	 */
	Matrix4x4 operator*(const Matrix4x4& other) const
	{
		Matrix4x4 result = {}; // 結果を格納する行列

		// 行列の各要素を計算
		for (int i = 0; i < 4; ++i)
		{
			for (int j = 0; j < 4; ++j)
			{
				result.m[i][j] = 0.0f;
				for (int k = 0; k < 4; ++k)
				{
					result.m[i][j] += m[i][k] * other.m[k][j];
				}
			}
		}

		return result;
	}

	/**
	 * @brief 行列とベクトルの掛け算
	 * @param vec 変換するベクトル
	 * @return 変換後のベクトル
	 */
	Vector4 operator*(const Vector4& vec) const {
		return {
			m[0][0] * vec.x + m[0][1] * vec.y + m[0][2] * vec.z + m[0][3] * vec.w,
			m[1][0] * vec.x + m[1][1] * vec.y + m[1][2] * vec.z + m[1][3] * vec.w,
			m[2][0] * vec.x + m[2][1] * vec.y + m[2][2] * vec.z + m[2][3] * vec.w,
			m[3][0] * vec.x + m[3][1] * vec.y + m[3][2] * vec.z + m[3][3] * vec.w
		};
	}
};

/**
 * @brief 4x4単位行列を生成
 * @return 単位行列
 */
inline Matrix4x4 MakeIdentity4x4()
{
	Matrix4x4 result = {
		1.0f,0.0f,0.0f,0.0f,
		0.0f,1.0f,0.0f,0.0f,
		0.0f,0.0f,1.0f,0.0f,
		0.0f,0.0f,0.0f,1.0f
	};
	return  result;
}

/**
 * @brief スケール行列を生成
 * @param scale 各軸のスケール値
 * @return スケール行列
 */
inline Matrix4x4 MakeScaleMatrix(const Vector3& scale)
{
	Matrix4x4 result = {
		scale.x,0.0f,0.0f,0.0f,
		0.0f,scale.y,0.0f,0.0f,
		0.0f,0.0f,scale.z,0.0f,
		0.0f,0.0f,0.0f,1.0f
	};
	return result;
}

/**
 * @brief 平行移動行列を生成
 * @param translate 平行移動ベクトル
 * @return 平行移動行列
 */
inline Matrix4x4 MakeTranslateMatrix(const Vector3& translate)
{
	Matrix4x4 result = {
		1.0f,0.0f,0.0f,0.0f,
		0.0f,1.0f,0.0f,0.0f,
		0.0f,0.0f,1.0f,0.0f,
		translate.x,translate.y,translate.z,1.0f
	};
	return result;
}

/**
 * @brief X軸回転行列を生成
 * @param radian 回転角度（ラジアン）
 * @return X軸回転行列
 */
inline Matrix4x4 MakeRotateXMatrix(float radian)
{
	Matrix4x4 m = {
		1,0,0,0,
		0,std::cos(radian),std::sin(radian),0,
		0,-std::sin(radian),std::cos(radian),0,
		0,0,0,1
	};
	return m;
}

/**
 * @brief Y軸回転行列を生成
 * @param radian 回転角度（ラジアン）
 * @return Y軸回転行列
 */
inline Matrix4x4 MakeRotateYMatrix(float radian)
{
	Matrix4x4 m = {
		std::cos(radian),0,-std::sin(radian),0,
		0,1,0,0,
		std::sin(radian),0,std::cos(radian),0,
		0,0,0,1
	};
	return m;
}

/**
 * @brief Z軸回転行列を生成
 * @param radian 回転角度（ラジアン）
 * @return Z軸回転行列
 */
inline Matrix4x4 MakeRotateZMatrix(float radian)
{
	Matrix4x4 m = {
		std::cos(radian),std::sin(radian),0,0,
		-std::sin(radian),std::cos(radian),0,0,
		0,0,1,0,
		0,0,0,1
	};
	return m;
}

/**
 * @brief 回転行列を生成（X→Y→Z順）
 * @param rotate 各軸の回転角度（ラジアン）
 * @return 合成された回転行列
 */
inline Matrix4x4 MakeRotateMatrix(const Vector3& rotate)
{
	// 各軸の回転行列を作成
	Matrix4x4 rotateXMatrix = MakeRotateXMatrix(rotate.x);
	Matrix4x4 rotateYMatrix = MakeRotateYMatrix(rotate.y);
	Matrix4x4 rotateZMatrix = MakeRotateZMatrix(rotate.z);

	// 回転行列の合成（X * Y * Z）
	Matrix4x4 rotateMatrix = rotateXMatrix * rotateYMatrix * rotateZMatrix;

	return rotateMatrix;
}

/**
 * @brief 4x4行列の和を計算
 * @param m1 行列1
 * @param m2 行列2
 * @return 行列の和
 */
//4x4行列の和
inline Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2)
{
	Matrix4x4 result;
	for (int row = 0; row < 4; row++)
	{
		for (int col = 0; col < 4; col++)
		{
			result.m[row][col] = m1.m[row][col] + m2.m[row][col];
		}
	}

	return  result;
}

/**
 * @brief 4x4行列の積を計算
 * @param m1 行列1
 * @param m2 行列2
 * @return 行列の積
 */
//4x4行列の積
inline Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2)
{
	Matrix4x4 result;
	result =
	{
		(m1.m[0][0] * m2.m[0][0]) + (m1.m[0][1] * m2.m[1][0]) + (m1.m[0][2] * m2.m[2][0]) + (m1.m[0][3] * m2.m[3][0]), (m1.m[0][0] * m2.m[0][1]) + (m1.m[0][1] * m2.m[1][1]) + (m1.m[0][2] * m2.m[2][1]) + (m1.m[0][3] * m2.m[3][1]), (m1.m[0][0] * m2.m[0][2]) + (m1.m[0][1] * m2.m[1][2]) + (m1.m[0][2] * m2.m[2][2]) + (m1.m[0][3] * m2.m[3][2]), (m1.m[0][0] * m2.m[0][3]) + (m1.m[0][1] * m2.m[1][3]) + (m1.m[0][2] * m2.m[2][3]) + (m1.m[0][3] * m2.m[3][3]),
		(m1.m[1][0] * m2.m[0][0]) + (m1.m[1][1] * m2.m[1][0]) + (m1.m[1][2] * m2.m[2][0]) + (m1.m[1][3] * m2.m[3][0]), (m1.m[1][0] * m2.m[0][1]) + (m1.m[1][1] * m2.m[1][1]) + (m1.m[1][2] * m2.m[2][1]) + (m1.m[1][3] * m2.m[3][1]), (m1.m[1][0] * m2.m[0][2]) + (m1.m[1][1] * m2.m[1][2]) + (m1.m[1][2] * m2.m[2][2]) + (m1.m[1][3] * m2.m[3][2]), (m1.m[1][0] * m2.m[0][3]) + (m1.m[1][1] * m2.m[1][3]) + (m1.m[1][2] * m2.m[2][3]) + (m1.m[1][3] * m2.m[3][3]),
		(m1.m[2][0] * m2.m[0][0]) + (m1.m[2][1] * m2.m[1][0]) + (m1.m[2][2] * m2.m[2][0]) + (m1.m[2][3] * m2.m[3][0]), (m1.m[2][0] * m2.m[0][1]) + (m1.m[2][1] * m2.m[1][1]) + (m1.m[2][2] * m2.m[2][1]) + (m1.m[2][3] * m2.m[3][1]), (m1.m[2][0] * m2.m[0][2]) + (m1.m[2][1] * m2.m[1][2]) + (m1.m[2][2] * m2.m[2][2]) + (m1.m[2][3] * m2.m[3][2]), (m1.m[2][0] * m2.m[0][3]) + (m1.m[2][1] * m2.m[1][3]) + (m1.m[2][2] * m2.m[2][3]) + (m1.m[2][3] * m2.m[3][3]),
		(m1.m[3][0] * m2.m[0][0]) + (m1.m[3][1] * m2.m[1][0]) + (m1.m[3][2] * m2.m[2][0]) + (m1.m[3][3] * m2.m[3][0]), (m1.m[3][0] * m2.m[0][1]) + (m1.m[3][1] * m2.m[1][1]) + (m1.m[3][2] * m2.m[2][1]) + (m1.m[3][3] * m2.m[3][1]), (m1.m[3][0] * m2.m[0][2]) + (m1.m[3][1] * m2.m[1][2]) + (m1.m[3][2] * m2.m[2][2]) + (m1.m[3][3] * m2.m[3][2]), (m1.m[3][0] * m2.m[0][3]) + (m1.m[3][1] * m2.m[1][3]) + (m1.m[3][2] * m2.m[2][3]) + (m1.m[3][3] * m2.m[3][3])
	};

	return result;
}

/**
 * @brief アフィン変換行列を生成
 * @param scale スケール
 * @param rotate 回転角度（ラジアン）
 * @param translate 平行移動
 * @return アフィン変換行列（Scale * Rotate * Translate）
 */
//アフィン返還
inline Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate)
{

	// スケール行列の生成
	Matrix4x4 scaleMatrix = {
		scale.x,0,0,0,
		0,scale.y,0,0,
		0,0,scale.z,0,
		0,0,0,1
	};


	// 回転で使う行列の宣言
	Matrix4x4 rotateMatrix;
	Matrix4x4 rotateXMatrix = MakeRotateXMatrix(rotate.x);		// X軸の回転
	Matrix4x4 rotateYMatrix = MakeRotateYMatrix(rotate.y);		// Y軸の回転
	Matrix4x4 rotateZMatrix = MakeRotateZMatrix(rotate.z);		// Z軸の回転

	// 回転の合成
	rotateMatrix = Multiply(rotateXMatrix, Multiply(rotateYMatrix, rotateZMatrix));


	// 平行移動行列の生成
	Matrix4x4 translateMatrix = {
		1,0,0,0,
		0,1,0,0,
		0,0,1,0,
		translate.x,translate.y,translate.z,1
	};


	// 3次元のアフィン変換
	Matrix4x4 weight = Multiply(Multiply(scaleMatrix, rotateMatrix), translateMatrix);

	// アフィン変換した値を返す
	return weight;

}

/**
 * @brief 逆行列を計算
 * @param m 入力行列
 * @return 逆行列
 */
inline Matrix4x4 Inverse(const Matrix4x4& m)
{
	Matrix4x4 result;
	float a;
	Matrix4x4 b;

	// 行列式の逆数を計算
	a = 1 / (m.m[0][0] * m.m[1][1] * m.m[2][2] * m.m[3][3] + m.m[0][0] * m.m[1][2] * m.m[2][3] * m.m[3][1] + m.m[0][0] * m.m[1][3] * m.m[2][1] * m.m[3][2]
		- m.m[0][0] * m.m[1][3] * m.m[2][2] * m.m[3][1] - m.m[0][0] * m.m[1][2] * m.m[2][1] * m.m[3][3] - m.m[0][0] * m.m[1][1] * m.m[2][3] * m.m[3][2]
		- m.m[0][1] * m.m[1][0] * m.m[2][2] * m.m[3][3] - m.m[0][2] * m.m[1][0] * m.m[2][3] * m.m[3][1] - m.m[0][3] * m.m[1][0] * m.m[2][1] * m.m[3][2]
		+ m.m[0][3] * m.m[1][0] * m.m[2][2] * m.m[3][1] + m.m[0][2] * m.m[1][0] * m.m[2][1] * m.m[3][3] + m.m[0][1] * m.m[1][0] * m.m[2][3] * m.m[3][2]
		+ m.m[0][1] * m.m[1][2] * m.m[2][0] * m.m[3][3] + m.m[0][2] * m.m[1][3] * m.m[2][0] * m.m[3][1] + m.m[0][3] * m.m[1][1] * m.m[2][0] * m.m[3][2]
		- m.m[0][3] * m.m[1][2] * m.m[2][0] * m.m[3][1] - m.m[0][2] * m.m[1][1] * m.m[2][0] * m.m[3][3] - m.m[0][1] * m.m[1][3] * m.m[2][0] * m.m[3][2]
		- m.m[0][1] * m.m[1][2] * m.m[2][3] * m.m[3][0] - m.m[0][2] * m.m[1][3] * m.m[2][1] * m.m[3][0] - m.m[0][3] * m.m[1][1] * m.m[2][2] * m.m[3][0]
		+ m.m[0][3] * m.m[1][2] * m.m[2][1] * m.m[3][0] + m.m[0][2] * m.m[1][1] * m.m[2][3] * m.m[3][0] + m.m[0][1] * m.m[1][3] * m.m[2][2] * m.m[3][0]);

	// 余因子行列を計算
	b.m[0][0] = m.m[1][1] * m.m[2][2] * m.m[3][3] + m.m[1][2] * m.m[2][3] * m.m[3][1] + m.m[1][3] * m.m[2][1] * m.m[3][2]
		- m.m[1][3] * m.m[2][2] * m.m[3][1] - m.m[1][2] * m.m[2][1] * m.m[3][3] - m.m[1][1] * m.m[2][3] * m.m[3][2];
	b.m[0][1] = -m.m[0][1] * m.m[2][2] * m.m[3][3] - m.m[0][2] * m.m[2][3] * m.m[3][1] - m.m[0][3] * m.m[2][1] * m.m[3][2]
		+ m.m[0][3] * m.m[2][2] * m.m[3][1] + m.m[0][2] * m.m[2][1] * m.m[3][3] + m.m[0][1] * m.m[2][3] * m.m[3][2];
	b.m[0][2] = m.m[0][1] * m.m[1][2] * m.m[3][3] + m.m[0][2] * m.m[1][3] * m.m[3][1] + m.m[0][3] * m.m[1][1] * m.m[3][2]
		- m.m[0][3] * m.m[1][2] * m.m[3][1] - m.m[0][2] * m.m[1][1] * m.m[3][3] - m.m[0][1] * m.m[1][3] * m.m[3][2];
	b.m[0][3] = -m.m[0][1] * m.m[1][2] * m.m[2][3] - m.m[0][2] * m.m[1][3] * m.m[2][1] - m.m[0][3] * m.m[1][1] * m.m[2][2]
		+ m.m[0][3] * m.m[1][2] * m.m[2][1] + m.m[0][2] * m.m[1][1] * m.m[2][3] + m.m[0][1] * m.m[1][3] * m.m[2][2];

	b.m[1][0] = -m.m[1][0] * m.m[2][2] * m.m[3][3] - m.m[1][2] * m.m[2][3] * m.m[3][0] - m.m[1][3] * m.m[2][0] * m.m[3][2]
		+ m.m[1][3] * m.m[2][2] * m.m[3][0] + m.m[1][2] * m.m[2][0] * m.m[3][3] + m.m[1][0] * m.m[2][3] * m.m[3][2];
	b.m[1][1] = m.m[0][0] * m.m[2][2] * m.m[3][3] + m.m[0][2] * m.m[2][3] * m.m[3][0] + m.m[0][3] * m.m[2][0] * m.m[3][2]
		- m.m[0][3] * m.m[2][2] * m.m[3][0] - m.m[0][2] * m.m[2][0] * m.m[3][3] - m.m[0][0] * m.m[2][3] * m.m[3][2];
	b.m[1][2] = -m.m[0][0] * m.m[1][2] * m.m[3][3] - m.m[0][2] * m.m[1][3] * m.m[3][0] - m.m[0][3] * m.m[1][0] * m.m[3][2]
		+ m.m[0][3] * m.m[1][2] * m.m[3][0] + m.m[0][2] * m.m[1][0] * m.m[3][3] + m.m[0][0] * m.m[1][3] * m.m[3][2];
	b.m[1][3] = +m.m[0][0] * m.m[1][2] * m.m[2][3] + m.m[0][2] * m.m[1][3] * m.m[2][0] + m.m[0][3] * m.m[1][0] * m.m[2][2]
		- m.m[0][3] * m.m[1][2] * m.m[2][0] - m.m[0][2] * m.m[1][0] * m.m[2][3] - m.m[0][0] * m.m[1][3] * m.m[2][2];

	b.m[2][0] = m.m[1][0] * m.m[2][1] * m.m[3][3] + m.m[1][1] * m.m[2][3] * m.m[3][0] + m.m[1][3] * m.m[2][0] * m.m[3][1]
		- m.m[1][3] * m.m[2][1] * m.m[3][0] - m.m[1][1] * m.m[2][0] * m.m[3][3] - m.m[1][0] * m.m[2][3] * m.m[3][1];
	b.m[2][1] = -m.m[0][0] * m.m[2][1] * m.m[3][3] - m.m[0][1] * m.m[2][3] * m.m[3][0] - m.m[0][3] * m.m[2][0] * m.m[3][1]
		+ m.m[0][3] * m.m[2][1] * m.m[3][0] + m.m[0][1] * m.m[2][0] * m.m[3][3] + m.m[0][0] * m.m[2][3] * m.m[3][1];
	b.m[2][2] = m.m[0][0] * m.m[1][1] * m.m[3][3] + m.m[0][1] * m.m[1][3] * m.m[3][0] + m.m[0][3] * m.m[1][0] * m.m[3][1]
		- m.m[0][3] * m.m[1][1] * m.m[3][0] - m.m[0][1] * m.m[1][0] * m.m[3][3] - m.m[0][0] * m.m[1][3] * m.m[3][1];
	b.m[2][3] = -m.m[0][0] * m.m[1][1] * m.m[2][3] - m.m[0][1] * m.m[1][3] * m.m[2][0] - m.m[0][3] * m.m[1][0] * m.m[2][1]
		+ m.m[0][3] * m.m[1][1] * m.m[2][0] + m.m[0][1] * m.m[1][0] * m.m[2][3] + m.m[0][0] * m.m[1][3] * m.m[2][1];

	b.m[3][0] = -m.m[1][0] * m.m[2][1] * m.m[3][2] - m.m[1][1] * m.m[2][2] * m.m[3][0] - m.m[1][2] * m.m[2][0] * m.m[3][1]
		+ m.m[1][2] * m.m[2][1] * m.m[3][0] + m.m[1][1] * m.m[2][0] * m.m[3][2] + m.m[1][0] * m.m[2][2] * m.m[3][1];
	b.m[3][1] = +m.m[0][0] * m.m[2][1] * m.m[3][2] + m.m[0][1] * m.m[2][2] * m.m[3][0] + m.m[0][2] * m.m[2][0] * m.m[3][1]
		- m.m[0][2] * m.m[2][1] * m.m[3][0] - m.m[0][1] * m.m[2][0] * m.m[3][2] - m.m[0][0] * m.m[2][2] * m.m[3][1];
	b.m[3][2] = -m.m[0][0] * m.m[1][1] * m.m[3][2] - m.m[0][1] * m.m[1][2] * m.m[3][0] - m.m[0][2] * m.m[1][0] * m.m[3][1]
		+ m.m[0][2] * m.m[1][1] * m.m[3][0] + m.m[0][1] * m.m[1][0] * m.m[3][2] + m.m[0][0] * m.m[1][2] * m.m[3][1];
	b.m[3][3] = m.m[0][0] * m.m[1][1] * m.m[2][2] + m.m[0][1] * m.m[1][2] * m.m[2][0] + m.m[0][2] * m.m[1][0] * m.m[2][1]
		- m.m[0][2] * m.m[1][1] * m.m[2][0] - m.m[0][1] * m.m[1][0] * m.m[2][2] - m.m[0][0] * m.m[1][2] * m.m[2][1];

	// 行列式の逆数と余因子行列の積を計算
	result.m[0][0] = a * b.m[0][0]; result.m[0][1] = a * b.m[0][1]; result.m[0][2] = a * b.m[0][2]; result.m[0][3] = a * b.m[0][3];
	result.m[1][0] = a * b.m[1][0]; result.m[1][1] = a * b.m[1][1]; result.m[1][2] = a * b.m[1][2]; result.m[1][3] = a * b.m[1][3];
	result.m[2][0] = a * b.m[2][0]; result.m[2][1] = a * b.m[2][1]; result.m[2][2] = a * b.m[2][2]; result.m[2][3] = a * b.m[2][3];
	result.m[3][0] = a * b.m[3][0]; result.m[3][1] = a * b.m[3][1]; result.m[3][2] = a * b.m[3][2]; result.m[3][3] = a * b.m[3][3];


	return result;
}

/**
 * @brief 正射影行列（Orthographic）を生成
 * @param left 左端
 * @param top 上端
 * @param right 右端
 * @param bottom 下端
 * @param nearClip 近クリップ面
 * @param farClip 遠クリップ面
 * @return 正射影行列
 */
inline Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip)
{
	Matrix4x4 m{
		2 / (right - left),0,0,0,
		0,2 / (top - bottom),0,0,
		0,0,1 / (farClip - nearClip),0,
		(left + right) / (left - right),(top + bottom) / (bottom - top),nearClip / (nearClip - farClip),1
	};

	return m;
}

/**
 * @brief 透視投影行列（Perspective）を生成
 * @param fovY 垂直視野角（ラジアン）
 * @param aspectRatio アスペクト比（幅/高さ）
 * @param nearClip 近クリップ面
 * @param farClip 遠クリップ面
 * @return 透視投影行列
 */
inline Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip)
{
	Matrix4x4 m{
	1 / aspectRatio * cot(fovY / 2), 0, 0, 0,
		0, cot(fovY / 2), 0, 0,
		0, 0, farClip / (farClip - nearClip), 1,
		0,0,(-nearClip * farClip) / (farClip - nearClip),0
	};
	return m;
}

/**
 * @brief ビューポート行列を生成
 * @param left 左端
 * @param top 上端
 * @param width 幅
 * @param height 高さ
 * @param minDepth 最小深度
 * @param maxDepth 最大深度
 * @return ビューポート行列
 */
inline Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth)
{
	Matrix4x4 m{
		width / 2,0,0,0,
		0,-height / 2,0,0,
		0,0,maxDepth - minDepth,0,
		left + (width / 2),top + (height / 2),minDepth,1
	};
	return m;
}

/**
 * @brief LookAt行列を生成（左手座標系）
 * @param eye カメラ（ライト）の位置
 * @param target 注視点
 * @param up 上方向ベクトル
 * @return ビュー行列
 */
inline Matrix4x4 MakeLookAtMatrix(const Vector3& eye, const Vector3& target, const Vector3& up)
{
	// Z軸（前方向）
	Vector3 zAxis = Vector3::Normalize(target - eye);
	// X軸（右方向）
	Vector3 xAxis = Vector3::Normalize(Vector3::Cross(up, zAxis));
	// Y軸（上方向）
	Vector3 yAxis = Vector3::Cross(zAxis, xAxis);

	Matrix4x4 result = {
		xAxis.x, yAxis.x, zAxis.x, 0.0f,
		xAxis.y, yAxis.y, zAxis.y, 0.0f,
		xAxis.z, yAxis.z, zAxis.z, 0.0f,
		-Vector3::Dot(xAxis, eye), -Vector3::Dot(yAxis, eye), -Vector3::Dot(zAxis, eye), 1.0f
	};

	return result;
}

/**
 * @brief 正射影行列を生成（シャドウマップ用、DirectX左手座標系）
 * @param width 幅
 * @param height 高さ
 * @param nearPlane 近クリップ面
 * @param farPlane 遠クリップ面
 * @return 正射影行列（深度は[0,1]に正規化）
 */
inline Matrix4x4 MakeOrthographicProjectionMatrix(float width, float height, float nearPlane, float farPlane)
{
	// DirectX標準の正射影行列（左手座標系、深度[0,1]）
	// https://docs.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixortholh
	float range = farPlane - nearPlane;

	Matrix4x4 result = {
		2.0f / width, 0.0f, 0.0f, 0.0f,
		0.0f, 2.0f / height, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f / range, 0.0f,
		0.0f, 0.0f, -nearPlane / range, 1.0f
	};

	return result;
}

