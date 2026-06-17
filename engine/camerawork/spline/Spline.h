#pragma once
#include "math/Vector3.h"

/**
 * @brief スプライン補間クラス
 * 
 * Catmull-Romスプライン補間を提供するユーティリティクラス。
 * 4つの制御点を使用して滑らかな曲線を生成する。
 */
class Spline
{
public:
	/**
	 * @brief Catmull-Romスプライン補間を計算
	 * 
	 * 4つの制御点と補間パラメータtから、曲線上の点を計算する。
	 * Catmull-Rom式は以下の通り：
	 * P(t) = 0.5 * ((2*P1) + (-P0+P2)*t + (2*P0-5*P1+4*P2-P3)*t^2 + (-P0+3*P1-3*P2+P3)*t^3)
	 * 
	 * @param p0 制御点0（曲線の手前の点、補間には直接使用されないが曲線の形状に影響）
	 * @param p1 制御点1（補間の開始点、t=0のとき返される）
	 * @param p2 制御点2（補間の終了点、t=1のとき返される）
	 * @param p3 制御点3（曲線の先の点、補間には直接使用されないが曲線の形状に影響）
	 * @param t 補間パラメータ（0.0〜1.0の範囲、0でp1、1でp2を返す）
	 * @return 補間された座標
	 */
	static Vector3 CatmullRom(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t);

};

