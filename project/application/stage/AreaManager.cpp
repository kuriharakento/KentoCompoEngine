#include "AreaManager.h"

AreaManager::AreaManager(const std::vector<std::shared_ptr<Area>>& areas) : areas_(areas), currentAreaIndex_(0), isAllCleared_(false)
{
}

void AreaManager::Start()
{
	// エリアが存在しない場合は早期リターン
	if (areas_.empty()) { return; };

	// 最初のエリアを開始
	StartCurrentArea();
}

void AreaManager::Update(CameraManager* camera)
{
	// 全エリアクリア済みの場合は更新不要
	if (isAllCleared_) { return; }

	// 現在のエリアを更新
	if (currentAreaIndex_ < areas_.size())
	{
		areas_[currentAreaIndex_]->Update(camera);
	}
}

void AreaManager::StartCurrentArea()
{
	// 全エリア終了チェック
	if (currentAreaIndex_ >= areas_.size())
	{
		// 全エリアクリア状態に設定
		isAllCleared_ = true;

		// エリアエフェクトを停止
		areaEffect_.Stop();

		// 全エリアクリアコールバックを実行
		if (onAllAreasCleared_) onAllAreasCleared_();
		return;
	}

	// 現在のエリアにクリアコールバックを設定
	// エリアクリア時に次のエリアへ進む
	areas_[currentAreaIndex_]->SetOnClearCallback([this]() {
		++currentAreaIndex_;
		StartCurrentArea();
												  });

	// エリアをアクティブ化（プレイヤー侵入検知を有効化）
	areas_[currentAreaIndex_]->SetActive(true);

	// エリアエフェクトの初期化と再生
	Area* currentArea = GetCurrentArea();
	if (currentArea)
	{
		GameObject* areaObj = currentArea->GetAreaObject();
		if (areaObj)
		{
			// エリアオブジェクトのトランスフォームでエフェクトを初期化
			areaEffect_.Initialize(areaObj->GetRotation(), areaObj->GetScale());
		}
	}
	areaEffect_.Play(areas_[currentAreaIndex_]->GetAreaObject()->GetPosition());

	// エリア開始コールバックを実行（演出制御用）
	if (onAreaStarted_)
	{
		onAreaStarted_(currentAreaIndex_, areas_[currentAreaIndex_].get());
	}

	// 前のエリアを非アクティブ化
	if (currentAreaIndex_ > 0)
	{
		areas_[currentAreaIndex_ - 1]->SetActive(false);
	}
}

