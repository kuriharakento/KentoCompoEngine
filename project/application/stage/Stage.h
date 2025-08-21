#pragma once
#include <vector>

#include "Area.h"

class Stage
{
public:


private:
	std::vector<Area> areas_; // エリアのリスト
	int currentAreaIndex_ = 0; // 現在のエリアインデックス
	int nextAreaIndex_ = 1; // 次のエリアインデックス

};

