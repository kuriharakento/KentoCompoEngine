#pragma once
#include "engine/scene/interface/AbstractSceneFactory.h"

class SceneFactory : public AbstractSceneFactory
{
public:
	
	/**
	 * \brief シーン生成
	 * \param name シーン名
	 * \return 生成したシーン
	 */
	BaseScene* CreateScene(const std::string& sceneName) override;
};

