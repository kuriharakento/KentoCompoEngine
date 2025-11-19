#pragma once
#include "camerawork/debug/DebugCamera.h"
#include "effects/particle/ParticleEmitter.h"
#include "scene/interface/BaseScene.h"

/**
 * @brief パーティクルテストシーン
 * 
 * パーティクルエフェクトの開発・検証用デバッグシーンです。
 * 様々なパーティクル効果（オーラ、煙、光など）をテストし、
 * パラメータ調整を行うための環境を提供します。
 * 
 * @note デバッグカメラで自由に視点を変更できます
 * @note 開発用シーンであり、本番ビルドには含まれません
 */
class ParticleTestScene : public BaseScene
{
public:
	/**
	 * @brief シーンの初期化処理
	 * 
	 * デバッグカメラ、テスト用パーティクルエミッターを初期化します。
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
	 * パーティクルエフェクトを描画します。
	 */
	void Draw3D() override;
	
	/**
	 * @brief 2D描画処理
	 * 
	 * デバッグ用UI要素を描画します（必要に応じて）。
	 */
	void Draw2D() override;

protected:
	/**
	 * @brief Playing状態開始時の処理
	 * 
	 * パーティクルテストの開始処理を行います。
	 */
	void OnEnterPlaying() override;
	
	/**
	 * @brief Playing状態の更新処理
	 * 
	 * デバッグカメラの更新、パーティクルエフェクトの更新、
	 * ImGuiによるパラメータ調整を行います。
	 */
	void OnUpdatePlaying() override;

private:
	// デバッグ用フリーカメラ
	std::unique_ptr<DebugCamera> debugCamera_;

	// オーラエフェクトのテスト用パーティクルエミッター
	// 白い円柱状のオーラ
	std::unique_ptr<ParticleEmitter> auraCylinder_;
	// モヤモヤとした煙のようなオーラ
	std::unique_ptr<ParticleEmitter> auraMist_;
	// 床に広がる光の効果
	std::unique_ptr<ParticleEmitter> auraFloor_;
	// 円柱から漏れる粒子
	std::unique_ptr<ParticleEmitter> auraLeak_;
};