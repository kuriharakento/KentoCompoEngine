#include <scene/MyGame.h>
#include <manager/graphics/TextureManager.h>

///=============================================================================
///						ゲームで使うリソースの読み込み
///=============================================================================

void MyGame::LoadTextures()
{
	TextureManager::GetInstance()->LoadTexture("./Resources/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/black.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/red.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/testSprite.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/white1x1.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/monsterBall.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/gradationLine.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/gradation.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/circle2.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/flowerfun.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/star.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/skybox.dds");
	TextureManager::GetInstance()->LoadTexture("./Resources/minimap_frame.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/numbers.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/title_logo.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/UI/hp_bar_fill.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/UI/hp_bar_frame.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/UI/reticle.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/UI/dot_reticle.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/UI/retry.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/UI/back_to_title.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/fonts/luna_atlas.png");
}

void MyGame::LoadModels()
{
	ModelManager::GetInstance()->LoadModel("multimaterial");
	ModelManager::GetInstance()->LoadModel("multimesh");
	ModelManager::GetInstance()->LoadModel("cube");
	ModelManager::GetInstance()->LoadModel("terrain");
	ModelManager::GetInstance()->LoadModel("skydome");
	ModelManager::GetInstance()->LoadModel("bullet");
	ModelManager::GetInstance()->LoadModel("wall");
	ModelManager::GetInstance()->LoadModel("player");
	ModelManager::GetInstance()->LoadModel("enemy");
	ModelManager::GetInstance()->LoadModel("plane", ".gltf");
	ModelManager::GetInstance()->LoadModel("walk", ".gltf");
	ModelManager::GetInstance()->LoadModel("street", ".gltf");
}
