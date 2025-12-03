#pragma once
#include "framework/Framework.h"

class MyGame : public Framework
{
public:
	//初期化
	void Initialize() override;

	//終了
	void Finalize() override;

	//毎フレーム
	void Update()override;

	//描画
	void Draw()override;
private:
	//テクスチャの読み込み
	void LoadTextures();
	//モデルの読み込み
	//モデルの読み込み
	void LoadModels();

private:
	// シーン描画用レンダーテクスチャ（ポストプロセス後）
	std::unique_ptr<RenderTexture> sceneRenderTexture_;
};

