#pragma once
#include "scene/interface/BaseScene.h"

/**
 * @brief ゲームクリアシーン
 * 
 * プレイヤーがステージをクリアした際に表示されるシーンです。
 * クリア画面の表示、リザルト情報、次のアクション（リトライ、タイトルへ戻る等）を提供します。
 * 
 * シーン状態遷移: Playing(クリア画面表示・入力待ち) → 次のシーンへ
 */
class GameClearScene : public BaseScene
{
public:
	/**
	 * @brief シーンの初期化処理
	 * 
	 * クリア画面のUI要素を初期化します。
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
	 * クリア画面のUI要素を描画します。
	 */
	void Draw2D() override;
	
	/**
	 * @brief ImGuiデバッグUI描画
	 * 
	 * 開発用のデバッグ情報を表示します。
	 */
	void DrawImGui() override;

protected:
	/**
	 * @brief Playing状態開始時の処理
	 * 
	 * クリア画面の表示を開始します。
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
};

