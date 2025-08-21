#pragma once
#include <vector>

#include "AreaInfo.h"
#include "jsonEditor/JsonEditableBase.h"

class AreaWaveData : public JsonEditableBase
{
public:
	AreaWaveData();
    // IJsonEditableインターフェース
    void DrawImGui() override;

	std::vector<AreaInfo> areas;
};

