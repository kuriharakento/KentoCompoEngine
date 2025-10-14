#pragma once
#include <vector>
#include <functional>
#include "Area.h"

class AreaManager
{
public:
    AreaManager(const std::vector<std::shared_ptr<Area>>& areas);

    void Start();

    void Update(CameraManager* camera);

    void SetOnAllAreasCleared(std::function<void()> cb) { onAllAreasCleared_ = std::move(cb); }
    bool IsAllCleared() const { return isAllCleared_; }
    int CurrentAreaIndex() const { return currentAreaIndex_; }
    Area* GetCurrentArea() const { return (currentAreaIndex_ < areas_.size()) ? areas_[currentAreaIndex_].get() : nullptr; }
    const std::vector<std::shared_ptr<Area>>& GetAreas() const { return areas_; }
    void SetOnAreaStarted(std::function<void(int, Area*)> cb) { onAreaStarted_ = std::move(cb); }

private:
    void StartCurrentArea();

private:
	// エリアリスト
    std::vector<std::shared_ptr<Area>> areas_;
	// 現在のエリアインデックス
	int currentAreaIndex_;
	// 全エリアクリアフラグ
    bool isAllCleared_;
	// 全エリアクリア時のコールバック
    std::function<void()> onAllAreasCleared_;
	// エリア開始時のコールバック
    std::function<void(int, Area*)> onAreaStarted_;
};