#pragma once
#include "scene/interface/BaseScene.h"
#include <application/effect/SceneTransitionEffect.h>
#include <application/UI/GameUI.h>
#include <graphics/2d/FontSprite.h>
#include <graphics/3d/Object3d.h>

/**
 * @brief ゲームオーバーシーン
 * 
 * プレイヤーが倒された際に表示されるシーンです。
 * ゲームオーバー画面の表示、次のアクション（リトライ、タイトルへ戻る等）を提供します。
 * 
 * シーン状態遷移: Enter(開始演出) → Playing(ゲームオーバー表示・入力待ち) → Exit(退場演出)
 */
class GameOverScene : public BaseScene
{
public:
	/**
	 * @brief シーンの初期化処理
	 * 
	 * ゲームオーバー画面のUI要素を初期化します。
	 */
	void Initialize() override;
	
	/**
	 * @brief シーンの終了処理
	 * 
	 * 各種リソースの解放を行います。
	 */
	void Finalize() override;
	
	/**
	 * @brief 3D描画処理
	 * 
	 * 3D要素を描画します（必要に応じて）。
	 */
	void Draw3D() override;
	
	/**
	 * @brief 2D描画処理
	 * 
	 * ゲームオーバー画面のUI要素を描画します。
	 */
	void Draw2D() override;
	
	/**
	 * @brief ImGuiデバッグUI描画
	 * 
	 * 開発用のデバッグ情報を表示します。
	 */
	void DrawImGui() override;

protected:
	// ==================================================
	// 状態フックのオーバーライド
	// ==================================================
	
	/**
	 * @brief Enter状態開始時の処理
	 * 
	 * シーン開始時の演出を開始します。
	 */
	void OnEnterEnter() override;
	
	/**
	 * @brief Enter状態の更新処理
	 * 
	 * 開始演出を進行させ、完了後にPlaying状態へ遷移します。
	 */
	void OnUpdateEnter() override;
	
	/**
	 * @brief Enter状態終了時の処理
	 * 
	 * Enter状態からの退場処理を行います。
	 */
	void OnExitEnter() override;

	/**
	 * @brief Playing状態開始時の処理
	 * 
	 * ゲームオーバー画面の表示を開始します。
	 */
	void OnEnterPlaying() override;
	
	/**
	 * @brief Playing状態の更新処理
	 * 
	 * プレイヤーの入力を監視し、次のアクションに応じてシーン遷移を行います。
	 */
	void OnUpdatePlaying() override;
	
	/**
	 * @brief Playing状態終了時の処理
	 * 
	 * Playing状態からの退場処理を行います。
	 */
	void OnExitPlaying() override;

	/**
	 * @brief Exit状態開始時の処理
	 * 
	 * シーン退場時の演出を開始します。
	 */
	void OnEnterExit() override;
	
	/**
	 * @brief Exit状態の更新処理
	 * 
	 * 退場演出を進行させ、完了後に次のシーンへ遷移します。
	 */
	void OnUpdateExit() override;
	
	/**
	 * @brief Exit状態終了時の処理
	 * 
	 * Exit状態からの退場処理を行います。
	 */
	void OnExitExit() override;

	/**
	 * @brief 共通更新処理
	 * 
	 * 各フレームで共通して行う更新処理を実装します。
	 */
	void CommonUpdate() override;

private: //メンバ変数
	// ゲームオーバーからタイトルUIの位置
	static constexpr Vector2 kGameOverToTitleUIPosition = { 360.0f, 580.0f };
	// ゲームオーバーからリトライUIの位置
	static constexpr Vector2 kGameOverRetryUIPosition = { 920.0f, 580.0f };
	// UIのサイズ
	static constexpr Vector2 kGameOverUISize = { 300.0f, 80.0f };
	// UIのアンカーポイント
	static constexpr Vector2 kGameOverUIAnchorPoint = { 0.5f, 0.5f };
	// タイトルフォントスプライトの位置
	static constexpr Vector2 kTitleFontSpritePosition = { 260.0f, 580.0f };
	// リトライフォントスプライトの位置
	static constexpr Vector2 kRetryFontSpritePosition = { 820.0f, 580.0f };
	// スカイドーム
	std::unique_ptr<Object3d> skydome_ = nullptr;
	// タイトルへ戻るフラグ
	bool returnToTitle_ = false;
	// リトライフラグ
	bool retry_ = false;
	// シーン遷移エフェクト（フェードイン/アウト）
	SceneTransitionEffect transitionEffect_;
	// ゲームオーバーからタイトルへ戻るUI
	std::unique_ptr<GameUI> gameOverToTitleUI_;
	// ゲームオーバーからリトライするUI
	std::unique_ptr<GameUI> gameOverRetryUI_;
	// ゲームオーバーロゴの文字
	std::unique_ptr<FontSprite> gameOverLogoFontSprite_;
	// タイトルの文字
	std::unique_ptr<FontSprite> titleFontSprite_;
	// リトライの文字
	std::unique_ptr<FontSprite> retryFontSprite_;
};

