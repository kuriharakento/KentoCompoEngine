#pragma once
#include <cassert>
#include <numbers>

#include "MatrixFunc.h"

/**
 * @brief 数学ユーティリティ関数を提供する名前空間
 * 
 * 乱数生成、行列操作、ベクトル変換、角度計算など、
 * 3Dグラフィックスやゲーム開発で頻繁に使用される数学関数を提供する。
 */
namespace MathUtils
{
	/**
	 * @brief 範囲[min, max]のランダムfloatを返す関数
	 * @param min 最小値
	 * @param max 最大値
	 * @return 指定範囲内のランダムな浮動小数点数
	 */
	float RandomFloat(float min, float max);

	/**
	 * @brief 範囲[min, max]のランダムVector3を返す関数
	 * @param min 各成分の最小値
	 * @param max 各成分の最大値
	 * @return 指定範囲内のランダムなVector3
	 */
	Vector3 RandomVector3(Vector3 min, Vector3 max);

	/**
	 * @brief 範囲[min, max]のランダムVector4を返す関数
	 * @param min 各成分の最小値
	 * @param max 各成分の最大値
	 * @return 指定範囲内のランダムなVector4
	 */
	Vector4 RandomVector4(Vector4 min, Vector4 max);

	/**
	 * @brief 行列から平行移動成分を取得
	 * @param matrix 4x4変換行列
	 * @return 平行移動ベクトル（x, y, z）
	 */
	Vector3 GetTranslateFromMatrix(const Matrix4x4& matrix);

	/**
	 * @brief 行列からスケール成分を取得
	 * @param matrix 4x4変換行列
	 * @return スケールベクトル（x, y, z）
	 */
	Vector3 GetScaleFromMatrix(const Matrix4x4& matrix);

	/**
	 * @brief 行列から回転成分をオイラー角で取得
	 * @param matrix 4x4変換行列
	 * @return 回転角度ベクトル（ラジアン）
	 */
	Vector3 GetRotateFromMatrix(const Matrix4x4& matrix);

	/**
	 * @brief 行列から回転成分を行列として取得
	 * @param matrix 4x4変換行列
	 * @return 回転のみの4x4行列
	 */
	Matrix4x4 GetMatrixRotate(const Matrix4x4& matrix);

	/**
	 * @brief 値を指定範囲内にクランプする
	 * @param value クランプする値
	 * @param min 最小値
	 * @param max 最大値
	 * @return クランプされた値
	 */
	float Clamp(float value, float min, float max);

	/**
	 * @brief 線形補間（Lerp）関数
	 * @param start 開始値
	 * @param end 終了値
	 * @param t 補間係数（0.0〜1.0）
	 * @return 補間された値
	 */
	static float Lerp(float start, float end, float t)
	{
		return start + (end - start) * t;
	}

	/**
	 * @brief Vector3の線形補間（Lerp）関数
	 * @param start 開始ベクトル
	 * @param end 終了ベクトル
	 * @param t 補間係数（0.0〜1.0）
	 * @return 補間されたベクトル
	 */
	static Vector3 Lerp(const Vector3& start, const Vector3& end, float t)
	{
		return start + (end - start) * t;
	}

	/**
	 * @brief 座標変換（ベクトルに行列を適用）
	 * @param vector 変換するベクトル
	 * @param matrix 変換行列
	 * @return 変換後のベクトル
	 */
	Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix);

	/**
	 * @brief 角度を -π 〜 π の範囲に正規化
	 * @param angle 正規化する角度（ラジアン）
	 * @return 正規化された角度
	 */
	static float NormalizeAngleRad(float angle)
	{
		while (angle > std::numbers::pi_v<float>) angle -= 2.0f * std::numbers::pi_v<float>;
		while (angle < -std::numbers::pi_v<float>) angle += 2.0f * std::numbers::pi_v<float>;
		return angle;
	}

	/**
	 * @brief 単一軸の角度補間（最短経路）
	 * @param start 開始角度（ラジアン）
	 * @param end 終了角度（ラジアン）
	 * @param t 補間係数（0.0〜1.0）
	 * @return 補間された角度
	 */
	static float LerpAngle(float start, float end, float t)
	{
		float delta = NormalizeAngleRad(end - start);
		return start + delta * t;
	}

	/**
	 * @brief Vector3の角度補間（各成分を最短経路で補間）
	 * @param from 開始角度ベクトル（ラジアン）
	 * @param to 終了角度ベクトル（ラジアン）
	 * @param t 補間係数（0.0〜1.0）
	 * @return 補間された角度ベクトル
	 */
	static Vector3 LerpAngle(const Vector3& from, const Vector3& to, float t) {
		return {
			LerpAngle(from.x, to.x, t),
			LerpAngle(from.y, to.y, t),
			LerpAngle(from.z, to.z, t)
		};
	}

	/**
	 * @brief 法線ベクトルの変換（平行移動成分を無視）
	 * @param normal 変換する法線ベクトル
	 * @param matrix 変換行列
	 * @return 変換後の法線ベクトル
	 */
	Vector3 TransformNormal(const Vector3& normal, const Matrix4x4& matrix);

	/**
	 * @brief 円軌道上の位置を計算
	 * @param center 円の中心座標
	 * @param radius 円の半径
	 * @param angle 角度（ラジアン）
	 * @return 円軌道上の位置（Y軸は中心と同じ）
	 */
	Vector3 CalculateOrbitPosition(const Vector3& center, float radius, float angle);

	/**
	 * @brief 方向ベクトルからYaw/Pitch角度を計算
	 * @param direction 方向ベクトル
	 * @return 回転角度ベクトル（pitch, yaw, 0）
	 */
	Vector3 CalculateYawPitchFromDirection(const Vector3& direction);

	/**
	 * @brief 現在位置からターゲット位置への向きを計算
	 * @param currentPosition 現在の位置
	 * @param targetPosition ターゲットの位置
	 * @return 向きの回転角度ベクトル（pitch, yaw, 0）
	 */
	Vector3 CalculateDirectionToTarget(const Vector3& currentPosition, const Vector3& targetPosition);

	/**
	 * @brief 転置行列を計算
	 * @param m 入力行列
	 * @return 転置された行列
	 */
	Matrix4x4 Transpose(const Matrix4x4& m);

};