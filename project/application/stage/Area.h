#pragma once
#include <vector>
#include <functional>
#include "WaveManager.h"
#include "application/GameObject/base/GameObject.h"

class EnemyManager;

class Area
{
public:
    Area(Object3dCommon* objCommon, LightManager* lightManager, EnemyManager* enemyManager, const std::vector<Wave>& waves);

    void Start();

    void Update(CameraManager* camera);

    void SetOnClearCallback(std::function<void()> cb) { onClearCallback_ = std::move(cb); }
    bool IsCleared() const { return isCleared_; }
    bool IsStarted() const { return isStarted_; }
	bool IsActive() const { return isActive_; }
	void SetActive(bool active) { isActive_ = active; }
	// エリアの判定用ゲームオブジェクトを取得
	GameObject* GetAreaObject() const { return areaObject_.get(); }

private:
    WaveManager waveManager_;
    // エリアの判定用ゲームオブジェクト
	std::unique_ptr<GameObject> areaObject_;
    bool isStarted_;
    bool isCleared_;
    bool isActive_;
    std::function<void()> onClearCallback_;
};