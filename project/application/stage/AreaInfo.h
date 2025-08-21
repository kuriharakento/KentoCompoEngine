#pragma once
#include "WaveInfo.h"

struct AreaInfo
{
    int areaIndex;
    std::vector<WaveInfo> waves;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AreaInfo, areaIndex, waves);