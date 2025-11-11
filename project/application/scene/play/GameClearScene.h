#pragma once
#include "scene/interface/BaseScene.h"

class GameClearScene : public BaseScene
{
public:
	// 初期化
	void Initialize() override;
	// 狩猟
	void Finalize() override;
	// 描画
	void Draw3D() override;
	void Draw2D() override;
	// ImGui の描画（BaseScene::DrawImGui をオーバーライド）
	void DrawImGui() override;

protected:
	// Playing: 実プレイ（ゲームプレイ)
	void OnEnterPlaying() override;
	void OnUpdatePlaying() override;
	void OnExitPlaying() override;
};

