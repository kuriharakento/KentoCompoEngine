#pragma once
#include <cassert>
#include <numbers>

#include "MatrixFunc.h"

namespace MathUtils
{
	// 範囲[min, max]のランダムfloatを返す関数
	float RandomFloat(float min, float max);
	Vector3 RandomVector3(Vector3 min, Vector3 max);
	Vector4 RandomVector4(Vector4 min, Vector4 max);

	//行列から成分を取得,設定する関数
	Vector3 GetMatrixTranslate(const Matrix4x4& matrix);
	Vector3 GetMatrixScale(const Matrix4x4& matrix);
	Vector3 GetMatrixRotate(const Matrix4x4& matrix);

	float Clamp(float value, float min, float max);

	// 線形補間（Lerp）関数
	static float Lerp(float start, float end, float t)
	{
		return start + (end - start) * t;
	}
	static Vector3 Lerp(const Vector3& start, const Vector3& end, float t)
	{
		return start + (end - start) * t;
	}

	///座標変換
	Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix);

	// 角度を -π ～ π の範囲に正規化
	static float NormalizeAngleRad(float angle)
	{
		while (angle > std::numbers::pi_v<float>) angle -= 2.0f * std::numbers::pi_v<float>;
		while (angle < -std::numbers::pi_v<float>) angle += 2.0f * std::numbers::pi_v<float>;
		return angle;
	}

	// 単一軸の角度補間（最短経路）
	static float LerpAngle(float start, float end, float t)
	{
		float delta = NormalizeAngleRad(end - start);
		return start + delta * t;
	}
	static Vector3 LerpAngle(const Vector3& from, const Vector3& to, float t) {
		return {
			LerpAngle(from.x, to.x, t),
			LerpAngle(from.y, to.y, t),
			LerpAngle(from.z, to.z, t)
		};
	}

	///法線ベクトルの変換
	Vector3 TransformNormal(const Vector3& normal, const Matrix4x4& matrix);

	///円軌道の計算
	Vector3 CalculateOrbitPosition(const Vector3& center, float radius, float angle);

	//回転角度の計算関数
	Vector3 CalculateYawPitchFromDirection(const Vector3& direction);

	// 現在位置とターゲット位置から向きを計算
	Vector3 CalculateDirectionToTarget(const Vector3& currentPosition, const Vector3& targetPosition);

	//転置行列
	Matrix4x4 Transpose(const Matrix4x4& m);

};