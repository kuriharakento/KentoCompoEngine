#pragma once
#include <memory>

// app
#include "application/GameObject/Combatable/character/enemy/EnemyManager.h"
#include "application/GameObject/Combatable/character/player/Player.h"
#include "application/GameObject/obstacle/ObstacleManager.h"
#include "application/stage/StageManager.h"
// camerawork
#include "camerawork/debug/DebugCamera.h"
#include "camerawork/spline/SplineCamera.h"
#include "camerawork/topDown/TopDownCamera.h"

// scene
#include "engine/scene/interface/BaseScene.h"

// graphics
#include "graphics/3d/Object3d.h"

// effects
#include "application/carnage/CarnageMode.h"
#include "application/effect/SceneTransitionEffect.h"
#include "application/effect/TitleFireEffect.h"
#include "application/minimap/Minimap.h"
#include "effects/particle/ParticleEmitter.h"
#include "graphics/2d/NumberSprite.h"
#include <graphics/2d/FontSprite.h>

/**
 * @brief タイトルシーン
 * 
 * ゲームのタイトル画面を表示し、プレイヤーの入力待ちを行います。
 * タイトルロゴ、背景演出、炎エフェクトなどの視覚効果を管理します。
 * 
 * シーン状態遷移: Playing(タイトル表示・入力待ち) → Exit(フェードアウト) → GamePlayScene
 * 
 * @note プレイヤーがスタートボタンを押すとGamePlaySceneへ遷移します
 */
class TitleScene : public BaseScene
{
public:
	/**
	 * @brief シーンの初期化処理
	 * 
	 * タイトルロゴ、スカイドーム、炎エフェクトなどの視覚要素を初期化し、
	 * BGMの再生を開始します。
	 */
	void Initialize() override;
	
	/**
	 * @brief シーンの終了処理
	 * 
	 * BGMの停止、各種リソースの解放を行います。
	 */
	void Finalize() override;
	
	/**
	 * @brief 3D描画処理
	 * 
	 * スカイドーム、装飾オブジェクトなどの3D要素を描画します。
	 */
	void Draw3D() override;
	
	/**
	 * @brief 2D描画処理
	 * 
	 * タイトルロゴ、UI要素を描画します。
	 */
	void Draw2D() override;
	
	/**
	 * @brief ImGuiデバッグUI描画
	 * 
	 * 開発用のデバッグ情報を表示します。
	 */
	void DrawImGui() override;

protected:
	/* ---- 状態ごとの関数のオーバーライド --- */
	
	/**
	 * @brief Playing状態開始時の処理
	 * 
	 * タイトル画面の表示を開始します。
	 */
	void OnEnterPlaying() override;
	
	/**
	 * @brief Playing状態の更新処理
	 * 
	 * プレイヤーの入力を監視し、スタートボタンが押されたらExit状態へ遷移します。
	 * 背景演出やエフェクトの更新も行います。
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
	 * GamePlaySceneへの遷移前のフェードアウト演出を開始します。
	 */
	void OnEnterExit() override;
	
	/**
	 * @brief Exit状態の更新処理
	 * 
	 * フェードアウト演出を進行させ、完了後にGamePlaySceneへ遷移します。
	 */
	void OnUpdateExit() override;
	
	/**
	 * @brief Exit状態終了時の処理
	 * 
	 * Exit状態からの退場処理を行います。
	 */
	void OnExitExit() override;

private: //メンバ変数
	// タイトルロゴ画像
	std::unique_ptr<Sprite> titleLogo_;
	// スカイドーム（背景天球）
	std::unique_ptr<Object3d> skydome_;
	// 炎エフェクト（タイトル演出用）
	std::unique_ptr<TitleFireEffect> fireEffect_;
	// 装飾用キューブ
	OBB cube_{};
	// キューブのY軸回転角度
	float cubeRotateY = 0.0f;
	// キューブの波動アニメーション用タイマー
	float cubeWaveTime = 0.0f;
	// シーン遷移エフェクト（フェードイン/アウト）
	SceneTransitionEffect transitionEffect_;
	// フォントスプライトを試してみる
	std::unique_ptr<FontSprite> fontSprite_;
};
