#pragma once
#include <cstdint>

namespace LightMaxCount
{
	//ポイントライトの最大数
	constexpr uint32_t kMaxPointLightCount = 100;
	//スポットライトの最大数
	constexpr uint32_t kMaxSpotLightCount = 100;
}

struct LightCount
{
	//ポイントライトの数
	uint32_t pointLightCount = 0;
	//スポットライトの数
	uint32_t spotLightCount = 0;
	//アライメント
	uint32_t padding;
};
