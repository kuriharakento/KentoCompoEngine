#pragma once

#include "BaseScene.h"
#include <string>

//シーン工場の抽象クラス
class AbstractSceneFactory
{
public:
	//仮想デストラクタ
	virtual ~AbstractSceneFactory() = default;
	//シーンの生成
	virtual BaseScene* CreateScene(const std::string& name) = 0;
};

