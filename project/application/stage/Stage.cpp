#include "Stage.h"

#include "graphics/3d/Object3dCommon.h"
#include "manager/editor/JsonEditorManager.h"


Stage::Stage(Object3dCommon* object3dCommon, LightManager* lightManager, EnemyManager* ememyManager, const std::string& filePath)
{
	// エリアウェーブデータを読み込み
	areaWaveData_ = std::make_shared<AreaWaveData>();
	areaWaveData_->LoadJson(filePath);

	// エディターにエリアウェーブデータを登録
	JsonEditorManager::GetInstance()->Register("areaWaveData", areaWaveData_);

	// エリア情報をもとにエリアを生成
	std::vector<std::shared_ptr<Area>> areas;
	for (const auto& areaInfo : areaWaveData_->areas)
	{
		std::vector<Wave> waves;
		for (const auto& waveInfo : areaInfo.waves)
		{
			std::vector<GameObjectInfo> enemies = waveInfo.enemies;
			waves.emplace_back(enemies);
		}
		auto area = std::make_shared<Area>(object3dCommon, lightManager, ememyManager, waves); // ここではnullptrを仮置き
		area->SetActive(false); // 初期状態では非アクティブ
		area->GetAreaObject()->SetPosition(areaInfo.areaTransform.translate);
		area->GetAreaObject()->SetRotation(areaInfo.areaTransform.rotate);
		area->GetAreaObject()->SetScale(areaInfo.areaTransform.scale);
		areas.push_back(area);
	}

	areaManager_ = std::make_unique<AreaManager>(areas);
	isCleared_ = false;
}

void Stage::Start()
{
    isCleared_ = false;
    areaManager_->SetOnAllAreasCleared([this]() {
        isCleared_ = true;
        if (onClearCallback_) onClearCallback_();
                                       });
    areaManager_->Start();
}

void Stage::Update(CameraManager* camera)
{
    if (!isCleared_)
    {
        areaManager_->Update(camera);
    }
}