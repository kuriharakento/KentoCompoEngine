#pragma once
#include "scene/interface/BaseScene.h"
#include <memory>
#include <vector>

// graphics
#include "graphics/3d/Object3d.h"
#include "graphics/3d/SkinnedObject3d.h"
// manager
#include "manager/scene/LightManager.h"
#include <camerawork/debug/DebugCamera.h>
#include <application/effect/SceneTransitionEffect.h>
#include <application/ui/GameUI.h>
#include <graphics/2d/FontSprite.h>

/**
 * @brief ゲームクリアシーン
 */
class GameClearScene : public BaseScene
{
public:
	void Initialize() override;
	void Finalize() override;
	void Draw2D() override;
	void Draw3D() override;
	void DrawShadow() override;
	void DrawGBuffer() override;
	void DrawImGui() override;

protected:
	/**
	 * @brief Enter状態開始時の処理
	 */
	void OnEnterEnter() override;

	/**
	 * @brief Enter状態の更新処理
	 */
	void OnUpdateEnter() override;

	/**
	 * @brief Enter状態終了時の処理
	 */
	void OnExitEnter() override;

	/**
	 * @brief Playing状態開始時の処理
	 */
	void OnEnterPlaying() override;

	/**
	 * @brief Playing状態の更新処理
	 */
	void OnUpdatePlaying() override;

	/**
	 * @brief Playing状態終了時の処理
	 */
	void OnExitPlaying() override;

	/**
	 * @brief Exit状態開始時の処理
	 */
	void OnEnterExit() override;

	/**
	 * @brief Exit状態の更新処理
	 */
	void OnUpdateExit() override;

	/**
	 * @brief Exit状態終了時の処理
	 */
	void OnExitExit() override;

	/**
	 * @brief 共通更新処理
	 */
	void CommonUpdate() override;

private:
	// カメラの初期方向
	static constexpr Vector3 kInitialCameraDirection = { 0.0f, -1.2f, 0.0f };
	// タイトルUIの位置
	static constexpr Vector2 kGameOverToTitleUIPosition = { 360.0f, 580.0f };
	// リトライUIの位置
	static constexpr Vector2 kGameOverRetryUIPosition = { 920.0f, 580.0f };
	// UIのサイズ
	static constexpr Vector2 kGameOverUISize = { 300.0f, 80.0f };
	// UIのアンカーポイント
	static constexpr Vector2 kGameOverUIAnchorPoint = { 0.5f, 0.5f };
	// タイトルフォントスプライトの位置
	static constexpr Vector2 kTitleFontSpritePosition = { 260.0f, 580.0f };
	// リトライフォントスプライトの位置
	static constexpr Vector2 kRetryFontSpritePosition = { 820.0f, 580.0f };
	// ゲームクリアロゴの位置
	static constexpr Vector2 kGameClearLogoPosition = { 250.0f, 200.0f };
	// フォントスケール
	static constexpr float kButtonFontScale = 0.5f;
	static constexpr float kLogoFontScale = 0.9f;
	// トランジション
	static constexpr int kTransitionGridX = 22;
	static constexpr int kTransitionGridY = 16;
	static constexpr float kTransitionDuration = 1.0f;
	// スカイドーム
	static constexpr float kSkydomeLightIntensity = 0.5f;
	// タイトルへ戻るフラグ
	bool returnToTitle_ = false;
	// リトライフラグ
	bool retry_ = false;
	// シーン遷移エフェクト（フェードイン/アウト）
	SceneTransitionEffect transitionEffect_;
	// タイトルへ戻るUI
	std::unique_ptr<GameUI> gameOverToTitleUI_;
	// リトライするUI
	std::unique_ptr<GameUI> gameOverRetryUI_;
	// ゲームクリアロゴの文字
	std::unique_ptr<FontSprite> gameClearLogoFontSprite_;
	// タイトルの文字
	std::unique_ptr<FontSprite> titleFontSprite_;
	// リトライの文字
	std::unique_ptr<FontSprite> retryFontSprite_;
};
