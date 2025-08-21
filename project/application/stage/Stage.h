#pragma once
#include <memory>
#include "AreaManager.h"
#include "AreaWaveData.h"

class Stage
{
public:
    Stage(Object3dCommon* object3dCommon, LightManager* lightManager, EnemyManager* ememyManager, const std::string& filePath);

    void Start();
    void Update();

    // ステージクリア時のコールバック
    void SetOnClearCallback(std::function<void()> cb) { onClearCallback_ = std::move(cb); }
    // 必要に応じて失敗時やリセット時のコールバックも追加

    bool IsCleared() const { return isCleared_; }
    AreaManager* GetAreaManager() const { return areaManager_.get(); }

private:
    std::unique_ptr<AreaManager> areaManager_;
    bool isCleared_ = false;
    std::function<void()> onClearCallback_;
	// エリアとそれごとのウェーブの情報
	std::shared_ptr<AreaWaveData> areaWaveData_; // エリアウェーブデータ
};
