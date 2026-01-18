#pragma once
#include <vector>

#include "Obstacle.h"
#include "ObstacleData.h"
#include "application/stage/StageData.h"
class CameraManager;
class LightManager;
class Object3dCommon;

class ObstacleManager
{
public:
	// 初期化
	void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager);
	// 更新
	// ロジック更新
	void Update();
	// 行列更新
	void UpdateTransforms(CameraManager* camera);
	// 描画
	void Draw(CameraManager* camera);
	// シャドウ描画
	void DrawShadow();

	void Clear();
	void CreateObstacles();
	void ApplyObstacleData();
	void LoadObstacleData(const std::string& path);
	void SetCulling(bool culling) { culling_ = culling; } // カリングの設定
	void SetObstacleData(ObstacleData* data);
	const std::vector<std::unique_ptr<Obstacle>>& GetObstacles() const { return obstacles_; }

private:
	void CreateObstacle(const GameObjectInfo& info);
	void CreateBarrierBlock(const GameObjectInfo& info);
	void CreateFloor(const GameObjectInfo& info);  // コライダーなしの床
	void SyncNewObstacleData();

private:
	Object3dCommon* object3dCommon_ = nullptr; // 3Dオブジェクト共通情報
	LightManager* lightManager_ = nullptr; // ライトマネージャー
	// 障害物配置データ
	ObstacleData* obstacleData_ = nullptr;
	// 障害物リスト
	std::vector<std::unique_ptr<Obstacle>> obstacles_;
	// カリング
	bool culling_ = false;
};

