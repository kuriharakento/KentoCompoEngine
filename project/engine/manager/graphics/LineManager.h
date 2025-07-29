#pragma once
#include <memory>

// system
#include "line/Line.h"
#include "line/LineCommon.h"
// math
#include "math/AABB.h"
#include "math/OBB.h"

class CameraManager;

class LineManager {
public:
	static LineManager* GetInstance();
    void Initialize(DirectXCommon* dxCommon,CameraManager* cameraManager);
    void RenderLines();
    void Clear();
	void Finalize();
	//ラインの描画
	void DrawLine(const Vector3& start, const Vector3& end, const Vector4& color);
	//キューブの描画
    void DrawCube(const Vector3& center, float size, const Vector4& color);
	//球の描画
	void DrawSphere(const Vector3& center, float radius, const Vector4& color);
	// グリッドの描画
    void DrawGrid(float gridSize, float gridSpacing, const Vector4& color);
	// 矢印の描画
	void DrawArrow(const Vector3& start, const Vector3& direction, float length, const Vector4& color);
	// 座標軸の描画
	void DrawAxis(const Vector3& position, float scale = 1.0f);
	// AABBの描画
	void DrawAABB(const AABB& aabb, const Vector4& color);
	// OBBの描画
	void DrawOBB(const OBB& obb, const Vector4& color);
	
private:
    std::unique_ptr<LineCommon> lineCommon_; ///< LineCommon クラスのインスタンス
    std::unique_ptr<Line> line_;             ///< Line クラスのインスタンス
    DirectXCommon* dxCommon_ = nullptr;      ///< DirectXCommon クラスのインスタンス
	CameraManager* cameraManager_ = nullptr; ///< CameraManager クラスのインスタンス

private:    //シングルトンインスタンス
	static LineManager* instance_; ///< シングルトンインスタンス
	LineManager() = default;        ///< コンストラクタ
	~LineManager() = default;       ///< デストラクタ
	LineManager(const LineManager&) = delete; ///< コピーコンストラクタ
	LineManager& operator=(const LineManager&) = delete; ///< 代入演算子
};
