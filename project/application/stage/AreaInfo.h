#pragma once
#include "WaveInfo.h"

struct AreaInfo
{
    int areaIndex;
    Transform areaTransform;
    std::vector<WaveInfo> waves;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AreaInfo, areaIndex, areaTransform, waves);