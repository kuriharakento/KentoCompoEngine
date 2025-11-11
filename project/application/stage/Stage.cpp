#include "Stage.h"

#include "graphics/3d/Object3dCommon.h"
#include "manager/editor/JsonEditorManager.h"
#include "engine/manager/graphics/LineManager.h"
#include "engine/math/VectorColorCodes.h"

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
		auto area = std::make_shared<Area>(object3dCommon, lightManager, ememyManager, waves);
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
#ifdef _DEBUG
        // デバッグ: JSON に定義された各エリアの各ウェーブの敵スポーン位置をワールド座標に変換して表示
        // areaWaveData_->areas の areaTransform.translate がエリアの原点（ワールド）、
        // enemy.transform.translate がエリア原点からのローカル位置である前提。
        for (const auto& areaInfo : areaWaveData_->areas)
        {
            // エリア原点（ワールド座標）
            Vector3 areaOrigin = areaInfo.areaTransform.translate;
            for (const auto& waveInfo : areaInfo.waves)
            {
                for (const auto& enemyInfo : waveInfo.enemies)
                {
                    // 大きさや色は自由に調整
                    LineManager::GetInstance()->DrawCube(
                        enemyInfo.transform.translate,
                        1.0f,
                        VectorColorCodes::Purple
                    );
                }
            }
        }
#endif
        areaManager_->Update(camera);
    }
}