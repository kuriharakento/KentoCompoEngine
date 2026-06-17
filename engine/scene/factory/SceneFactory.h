#pragma once
#include "engine/scene/interface/AbstractSceneFactory.h"
#include <memory>

class SceneFactory : public AbstractSceneFactory
{
public:
	
	/**
	 * \brief シーン生成
	 * \param name シーン名
	 * \return 生成したシーン
	 */
	std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) override;
};

