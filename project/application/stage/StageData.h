#pragma once
// editor
#include "GameObjectInfo.h"
#include "engine/jsonEditor/JsonEditableBase.h"

class StageData : public JsonEditableBase
{
public:
	StageData();
	void DrawImGui() override;
	std::vector<GameObjectInfo> gameObjects; // ゲームオブジェクトのリスト
};

