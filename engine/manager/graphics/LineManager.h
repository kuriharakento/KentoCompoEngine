#pragma once
#include <memory>

// system
#include "line/Line.h"
#include "line/LineCommon.h"
// math
#include "math/AABB.h"
#include "math/OBB.h"

class CameraManager;

/**
 * @brief ライン描画マネージャークラス
 * @details デバッグ描画用のユーティリティクラス
 *          線、キューブ、球、グリッド、矢印、座標軸、AABB、OBBの描画機能を提供
 */
class LineManager {
public:
	/**
	 * @brief シングルトンインスタンスを取得
	 * @return LineManagerのインスタンス
	 */
	static LineManager* GetInstance();

	/**
	 * @brief 初期化処理
	 * @param dxCommon DirectXCommonへのポインタ
	 * @param cameraManager カメラマネージャーへのポインタ
	 */
    void Initialize(DirectXCommon* dxCommon,CameraManager* cameraManager);

	/**
	 * @brief ラインの描画処理
	 * @details 登録されたすべてのラインを描画し、描画後にクリアする
	 */
    void RenderLines();

	/**
	 * @brief 登録されたラインをクリア
	 */
    void Clear();

	/**
	 * @brief 終了処理
	 */
	void Finalize();

	/**
	 * @brief ラインの描画
	 * @param start 開始点
	 * @param end 終了点
	 * @param color 色
	 */
	void DrawLine(const Vector3& start, const Vector3& end, const Vector4& color);

	/**
	 * @brief キューブの描画
	 * @param center 中心位置
	 * @param size サイズ
	 * @param color 色
	 * @details 8頂点12辺のワイヤーフレームキューブを描画する
	 */
    void DrawCube(const Vector3& center, float size, const Vector4& color);

	/**
	 * @brief 球の描画
	 * @param center 中心位置
	 * @param radius 半径
	 * @param color 色
	 * @details 緯度・経度方向のラインで球を描画する
	 */
	void DrawSphere(const Vector3& center, float radius, const Vector4& color);

	/**
	 * @brief グリッドの描画
	 * @param gridSize グリッドの全体サイズ
	 * @param gridSpacing グリッドの間隔
	 * @param color 色
	 * @details XZ平面にグリッドを描画する
	 */
    void DrawGrid(float gridSize, float gridSpacing, const Vector4& color);

	/**
	 * @brief 矢印の描画
	 * @param start 開始点
	 * @param direction 方向ベクトル
	 * @param length 長さ
	 * @param color 色
	 */
	void DrawArrow(const Vector3& start, const Vector3& direction, float length, const Vector4& color);

	/**
	 * @brief 座標軸の描画
	 * @param position 描画位置
	 * @param scale スケール（デフォルト: 1.0f）
	 * @details X軸（赤）、Y軸（緑）、Z軸（青）を描画する
	 */
	void DrawAxis(const Vector3& position, float scale = 1.0f);

	/**
	 * @brief AABBの描画
	 * @param aabb 描画するAABB
	 * @param color 色
	 */
	void DrawAABB(const AABB& aabb, const Vector4& color);

	/**
	 * @brief OBBの描画
	 * @param obb 描画するOBB
	 * @param color 色
	 */
	void DrawOBB(const OBB& obb, const Vector4& color);
	
private:
    std::unique_ptr<LineCommon> lineCommon_; // LineCommonクラスのインスタンス
    std::unique_ptr<Line> line_;             // Lineクラスのインスタンス
    DirectXCommon* dxCommon_ = nullptr;      // DirectXCommonへのポインタ
	CameraManager* cameraManager_ = nullptr; // CameraManagerへのポインタ

private:    // シングルトンインスタンス
	static std::unique_ptr<LineManager> instance_;
	LineManager() = default;                                 // コンストラクタ
	LineManager(const LineManager&) = delete;                // コピーコンストラクタ
	LineManager& operator=(const LineManager&) = delete;     // コピー代入禁止

public:
	~LineManager() = default;                                // デストラクタ演算子
};
