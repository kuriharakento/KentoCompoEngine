#pragma once

#include "BaseScene.h"
#include <string>
#include <memory>

namespace KCE
{
//シーン工場の抽象クラス
class AbstractSceneFactory
{
public:
	//仮想デストラクタ
	virtual ~AbstractSceneFactory() = default;
	//シーンの生成
	virtual std::unique_ptr<BaseScene> CreateScene(const std::string& name) = 0;
};
} // namespace KCE
