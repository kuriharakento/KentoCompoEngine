#pragma once
#include <vector>

#include "Obstacle.h"
#include "ObstacleData.h"
class CameraManager;
class LightManager;
class Object3dCommon;

class ObstacleManager
{
public:
	// 初期化
	void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager);
	// 更新
	void Update();
	// 描画
	void Draw(CameraManager* camera);

	void LoadObstacleData(const std::string& jsonName);
	void CreateObstacles(const std::string& modelName);
	void ApplyObstacleData();
	void SetCulling(bool culling) { culling_ = culling; } // カリングの設定

private:
	Object3dCommon* object3dCommon_ = nullptr; // 3Dオブジェクト共通情報
	LightManager* lightManager_ = nullptr; // ライトマネージャー
	// 障害物配置データ
	std::shared_ptr<ObstacleData> obstacleData_;
	// 障害物リスト
	std::vector<std::unique_ptr<Obstacle>> obstacles_;
	// カリング
	bool culling_ = true;
};

