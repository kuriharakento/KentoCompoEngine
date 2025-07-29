#include "engine/scene/MyGame.h"

//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	//フレームワーク
	Framework* game = new MyGame();

	//実行
	game->Run();
  
	//解放
	delete game;

	//終了
	return 0;
}