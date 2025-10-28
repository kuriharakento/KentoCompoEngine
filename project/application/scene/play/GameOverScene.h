#pragma once
#include "scene/interface/BaseScene.h"

class GameOverScene : public BaseScene
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
	// 状態フックのオーバーライド
	// シーン開始
	void OnEnterEnter() override;
	void OnUpdateEnter() override;
	void OnExitEnter() override;

	// プレイ
	void OnEnterPlaying() override;
	void OnUpdatePlaying() override;
	void OnExitPlaying() override;

	// 終了
	void OnEnterExit() override;
	void OnUpdateExit() override;
	void OnExitExit() override;
};

