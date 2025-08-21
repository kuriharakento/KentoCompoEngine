#pragma once
#include <vector>

#include "GameObjectInfo.h"

struct WaveInfo
{
	std::vector<GameObjectInfo> enemies; // ウェーブに含まれる敵の情報
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(WaveInfo, enemies)
