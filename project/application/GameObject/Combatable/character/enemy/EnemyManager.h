#pragma once
#include "application/effect/EnemyDeathEffect.h"
#include "application/stage/StageData.h"
#include "base/EnemyBase.h"
#include "math/AABB.h"
#include <vector>
#include <memory>

class LightManager;
class Object3dCommon;

/**
 * @brief 敵キャラクターの生成と管理を行うマネージャークラス
 * 
 * ステージ内の全ての敵キャラクターのライフサイクルを管理します。
 * 敵の生成、更新、描画、破棄を一元管理し、ウェーブシステムと連携します。
 * 
 * 主な機能:
 * - 敵の種類別生成（アサルト、ピストル、ショットガン、ナイフ）
 * - 全滅判定とコールバック通知
 * - 遅延破棄システム（描画中の不正アクセス防止）
 * - 死亡エフェクト管理
 * - JSONからの敵配置読み込み
 * 
 * @note 敵の破棄は即座に行わず、Draw終了後に遅延破棄されます
 */
class EnemyManager
{
public:
	/**
	 * @brief 敵マネージャーの初期化
	 * @param object3dCommon 3Dオブジェクト共通データ
	 * @param lightManager ライト管理クラス
	 * @param target 敵が追跡する対象（通常はプレイヤー）
	 */
	void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, GameObject* target);
	
	/**
	 * @brief 毎フレームの更新処理
	 * 
	 * 全ての敵を更新し、死亡した敵の処理を行います。
	 * 全滅時にはコールバック関数を呼び出します。
	 */
	void Update();
	
	/**
	 * @brief トランスフォーム情報の更新
	 * @param camera カメラ管理クラス
	 */
	void UpdateTransform(CameraManager* camera);
	
	/**
	 * @brief 描画処理
	 * 
	 * 全ての敵を描画し、描画終了後に遅延破棄リストをクリーンアップします。
	 * 
	 * @param camera カメラ管理クラス
	 */
	void Draw(CameraManager* camera);
	
	/**
	 * @brief 敵リストを取得
	 * @return 現在アクティブな敵のリスト
	 */
	const std::vector<std::unique_ptr<EnemyBase>>& GetEnemies() const { return enemies_; }
	
	/**
	 * @brief 敵の数を取得
	 * @return 現在の敵の数
	 */
	uint32_t GetEnemyCount() const { return static_cast<uint32_t>(enemies_.size()); }
	
	/**
	 * @brief ピストル敵を追加
	 * @param count 追加する敵の数
	 */
	void AddPistolEnemy(uint32_t count);
	
	/**
	 * @brief アサルト敵を追加
	 * @param count 追加する敵の数
	 */
	void AddAssaultEnemy(uint32_t count);
	
	/**
	 * @brief ショットガン敵を追加
	 * @param count 追加する敵の数
	 */
	void AddShotgunEnemy(uint32_t count);
	
	/**
	 * @brief ナイフ敵を追加
	 * @param count 追加する敵の数
	 */
	void AddKnifeEnemy(uint32_t count);
	
	/**
	 * @brief 敵データを設定（古いメソッド）
	 * @param data ゲームオブジェクト情報のリスト
	 */
	void SetEnemyData(const std::vector<GameObjectInfo>& data);
	
	/**
	 * @brief JSONデータから敵を生成
	 * @param data ゲームオブジェクト情報のリスト
	 */
	void AddEnemiesFromGameObjectInfo(const std::vector<GameObjectInfo>& data);
	
	/**
	 * @brief 追跡対象を設定
	 * @param target 敵が追跡する対象（通常はプレイヤー）
	 */
	void SetTarget(GameObject* target) { target_ = target; }
	
	/**
	 * @brief カメラマネージャーを設定
	 * @param camera カメラ管理クラス（シェイク演出用）
	 */
	void SetCameraManager(CameraManager* camera) { camera_ = camera; }
	
	/**
	 * @brief 全敵撃破時のコールバックを設定
	 * 
	 * ウェーブシステムが次のウェーブへ進むために使用します。
	 * 
	 * @param callback 全敵撃破時に呼ばれる関数
	 */
	void SetOnAllEnemiesDefeatedCallback(std::function<void()> callback) { onAllEnemiesDefeatedCallback_ = std::move(callback); }
	
	/**
	 * @brief 全ての敵をクリア
	 */
	void Clear();

private:
	/**
	 * @brief 敵データからアサルト敵を生成
	 */
	void CreateAssaultEnemyFromData();

	/**
	 * @brief 遅延破棄リストのクリーンアップ
	 * 
	 * Draw終了後に呼び出され、死亡した敵を安全に破棄します。
	 */
	void CleanupPendingRemovals();

private:
	// 3Dオブジェクト共通データ
	Object3dCommon* object3dCommon_ = nullptr;
	
	// ライト管理
	LightManager* lightManager_ = nullptr;
	
	// 敵が追跡する対象（プレイヤー）
	GameObject* target_ = nullptr;
	
	// カメラ管理（シェイク演出用）
	CameraManager* camera_ = nullptr;
	
	// 敵の出現範囲
	AABB emitRange_ = {};
	
	// 現在アクティブな敵のリスト
	std::vector<std::unique_ptr<EnemyBase>> enemies_;
	
	// 遅延破棄待ちの敵のリスト（描画中の不正アクセス防止）
	std::vector<std::unique_ptr<EnemyBase>> pendingRemovals_;
	
	// 敵データ（JSONから読み込み）
	std::vector<GameObjectInfo> enemyData_;
	
	// 死亡エフェクト管理
	std::unique_ptr<EnemyDeathEffect> deathEffect_;
	
	// 全敵撃破時のコールバック
	std::function<void()> onAllEnemiesDefeatedCallback_ = nullptr;
};

