#include "AreaManager.h"

AreaManager::AreaManager(const std::vector<std::shared_ptr<Area>>& areas) : areas_(areas), currentAreaIndex_(0), isAllCleared_(false)
{
}

void AreaManager::Start()
{
	if (areas_.empty()) { return; };
	StartCurrentArea();
}

void AreaManager::Update(CameraManager* camera)
{
	if (isAllCleared_) { return; }
	if (currentAreaIndex_ < areas_.size())
	{
		areas_[currentAreaIndex_]->Update(camera);
	}
}

void AreaManager::StartCurrentArea()
{
	if (currentAreaIndex_ >= areas_.size())
	{
		isAllCleared_ = true;
		// エフェクトの停止
		areaEffect_.Stop();
		if (onAllAreasCleared_) onAllAreasCleared_();
		return;
	}

	areas_[currentAreaIndex_]->SetOnClearCallback([this]() {
		++currentAreaIndex_;
		StartCurrentArea();
												  });
	areas_[currentAreaIndex_]->SetActive(true); // エリアをアクティブにする
	// エリアエフェクトを再生
	Area* currentArea = GetCurrentArea();
	if (currentArea)
	{
		GameObject* areaObj = currentArea->GetAreaObject();
		if (areaObj)
		{
			areaEffect_.Initialize(areaObj->GetRotation(), areaObj->GetScale());
		}
	}
	areaEffect_.Play(areas_[currentAreaIndex_]->GetAreaObject()->GetPosition());
	// エリア開始のコールバックを呼び出す
	if (onAreaStarted_)
	{
		onAreaStarted_(currentAreaIndex_, areas_[currentAreaIndex_].get());
	}
	if (currentAreaIndex_ > 0)
	{
		// 前のエリアを非アクティブにする
		areas_[currentAreaIndex_ - 1]->SetActive(false);
	}
}

