#pragma once
#include <vector>

#include "base/GraphicsTypes.h"

/**
 * @brief パーティクル形状生成用の数学関数群
 * 
 * 各種3D形状の頂点データを生成するための関数を提供する名前空間。
 * 生成される頂点データはGPUに転送され、インスタンシング描画で使用される。
 */
namespace ParticleMath
{
	/**
	 * @brief 平面（矩形）の頂点データを生成する
	 * @return 平面の頂点データ配列
	 */
	std::vector<VertexData> MakePlaneVertexData();

	/**
	 * @brief リング（輪）の頂点データを生成する
	 * @return リングの頂点データ配列（32分割）
	 */
	std::vector<VertexData> MakeRingVertexData();

	/**
	 * @brief 円柱の頂点データを生成する
	 * @return 円柱の頂点データ配列（32分割）
	 */
	std::vector<VertexData> MakeCylinderVertexData();

	/**
	 * @brief 球体の頂点データを生成する
	 * @return 球体の頂点データ配列（緯度16分割、経度32分割）
	 */
	std::vector<VertexData> MakeSphereVertexData();

	/**
	 * @brief トーラス（ドーナツ形状）の頂点データを生成する
	 * @return トーラスの頂点データ配列（外周32分割、断面16分割）
	 */
	std::vector<VertexData> MakeTorusVertexData();

	/**
	 * @brief 星型の頂点データを生成する
	 * @return 星型の頂点データ配列（5頂点）
	 */
	std::vector<VertexData> MakeStarVertexData();

	/**
	 * @brief ハート型の頂点データを生成する
	 * @return ハート型の頂点データ配列
	 */
	std::vector<VertexData> MakeHeartVertexData();

	/**
	 * @brief スパイラル（螺旋）の頂点データを生成する
	 * @return スパイラルの頂点データ配列
	 */
	std::vector<VertexData> MakeSpiralVertexData();

	/**
	 * @brief 円錐の頂点データを生成する
	 * @return 円錐の頂点データ配列（32分割）
	 */
	std::vector<VertexData> MakeConeVertexData();

	/**
	 * @brief 立方体の頂点データを生成する
	 * @return 立方体の頂点データ配列
	 */
	std::vector<VertexData> MakeCubeVertexData();
}
