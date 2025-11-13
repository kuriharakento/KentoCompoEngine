#pragma once

// system
#include "graphics/3d/Object3dCommon.h"
#include "manager/scene/CameraManager.h"
#include "manager/scene/LightManager.h"

// app
#include "Stage.h"
#include "StageData.h"
#include "application/GameObject/Combatable/character/enemy/EnemyManager.h"
#include "application/GameObject/Combatable/character/player/Player.h"
#include "application/GameObject/obstacle/ObstacleManager.h"

/**
 * @brief ステージ全体とゲームオブジェクトを統合管理するマネージャークラス
 * 
 * ゲーム内のステージ全体の初期化、更新、描画を一元管理します。
 * JSONファイルからステージデータを読み込み、プレイヤー、敵、障害物、
 * ステージ構造（エリア・ウェーブ）などを生成・管理します。
 * 
 * 責任範囲:
 * - ステージデータのJSONファイル読み込み
 * - ゲームオブジェクト（プレイヤー、敵、障害物）の生成と管理
 * - Stageクラスによるエリア・ウェーブ進行の制御
 * - デバッグUI（ImGui）による編集・制御機能
 * 
 * 管理するオブジェクト:
 * - Player: プレイヤー（1体）
 * - EnemyManager: 敵の管理
 * - ObstacleManager: 障害物の管理
 * - Stage: エリア・ウェーブの進行制御
 * 
 * 使用例:
 * @code
 * // ステージマネージャーの初期化
 * StageManager stageManager;
 * stageManager.Initialize(objCommon, lightManager, cameraManager);
 * 
 * // ステージをロード
 * stageManager.LoadStage("field");
 * 
 * // ゲームループ
 * while (gameRunning) {
 *     stageManager.Update();
 *     stageManager.UpdateTransforms(cameraManager);
 *     stageManager.Draw();
 * }
 * @endcode
 */
class StageManager
{
public:
	/**
	 * @brief コンストラクタ
	 */
	StageManager();

	/**
	 * @brief デストラクタ
	 * 
	 * 管理している全てのゲームオブジェクトとデータを解放します。
	 */
	~StageManager();

	/**
	 * @brief ステージマネージャーの初期化
	 * 
	 * 各マネージャー（敵、障害物）の初期化と、
	 * JSON編集システムへのデータ登録を行います。
	 * 
	 * @param object3dCommon 3Dオブジェクト共通データへのポインタ
	 * @param lightManager ライトマネージャーへのポインタ
	 * @param camera カメラマネージャーへのポインタ
	 */
	void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, CameraManager* camera);

	/**
	 * @brief ステージマネージャーの更新処理
	 * 
	 * プレイヤー、敵、障害物、ステージの更新を行います。
	 * デバッグモードでは、障害物データの同期も行います。
	 * 毎フレーム呼び出す必要があります。
	 */
	void Update();

	/**
	 * @brief トランスフォーム（行列）の更新
	 * 
	 * 全てのゲームオブジェクトの行列計算を行います。
	 * Update()の後、Draw()の前に呼び出す必要があります。
	 * 
	 * @param camera カメラマネージャーへのポインタ（ビュー行列の計算に使用）
	 */
	void UpdateTransforms(CameraManager* camera);

	/**
	 * @brief ステージの描画
	 * 
	 * プレイヤー、敵、障害物の描画を行います。
	 */
	void Draw();

	/**
	 * @brief ImGuiによるデバッグUI表示
	 * 
	 * ステージ管理用のデバッグUIを表示します。
	 * デバッグビルド時のみ有効です（USE_IMGUIマクロで制御）。
	 * 
	 * 機能:
	 * - 敵の全クリア
	 * - 障害物の全クリア
	 * - ステージのロード
	 */
	void DrawImGui();

	/**
	 * @brief ステージデータをJSONファイルから読み込む
	 * 
	 * 指定されたステージ名のディレクトリから、ステージデータと
	 * エリア・ウェーブデータを読み込み、ゲームオブジェクトを生成します。
	 * 
	 * 読み込まれるファイル:
	 * - stage/{stageName}.json: ステージ上の固定オブジェクト配置
	 * - stage/{stageName}_area.json: エリアとウェーブの定義
	 * 
	 * @param stageName ステージ名（ディレクトリ名）
	 */
	void LoadStage(const std::string& stageName);

	/**
	 * @brief ステージデータをもとに各ゲームオブジェクトの情報を分類・生成
	 * 
	 * StageDataから読み込んだGameObjectInfoリストを、
	 * タイプごとに分類して対応するマネージャーに渡します。
	 * 
	 * 処理内容:
	 * - "PlayerSpawn": プレイヤーの生成
	 * - "Obstacle", "BarrierBlock": 障害物の生成
	 * - その他のタイプは必要に応じて追加可能
	 */
	void CreateInfosFromStageData();

	/**
	 * @brief ステージがクリアされているかを取得
	 * @return true: ステージクリア済み, false: 未クリア
	 */
	bool IsStageCleared() const { return stage_ ? stage_->IsCleared() : false; }

	/**
	 * @brief プレイヤーオブジェクトを取得
	 * @return プレイヤーへのポインタ（未生成の場合はnullptr）
	 */
	Player* GetPlayer() const { return player_.get(); }

	/**
	 * @brief 敵マネージャーを取得
	 * @return EnemyManagerへのポインタ
	 */
	EnemyManager* GetEnemyManager() const { return enemyManager_.get(); }

	/**
	 * @brief 障害物マネージャーを取得
	 * @return ObstacleManagerへのポインタ
	 */
	ObstacleManager* GetObstacleManager() const { return obstacleManager_.get(); }

	/**
	 * @brief ステージオブジェクトを取得
	 * @return Stageへのポインタ（未生成の場合はnullptr）
	 */
	Stage* GetStage() const { return stage_.get(); }

private:
	Object3dCommon* object3dCommon_;  ///< 3Dオブジェクトの共通データへのポインタ
	LightManager* lightManager_;      ///< ライトマネージャーへのポインタ
	CameraManager* cameraManager_;    ///< カメラマネージャーへのポインタ

	std::shared_ptr<StageData> stageData_;      ///< ステージデータ（固定オブジェクト配置）
	std::shared_ptr<ObstacleData> obstacleData_; ///< 障害物データ

	// -------- ゲームオブジェクト -------- //

	std::unique_ptr<Player> player_;               ///< プレイヤー（1体のみ）
	std::unique_ptr<EnemyManager> enemyManager_;   ///< 敵マネージャー
	std::unique_ptr<ObstacleManager> obstacleManager_; ///< 障害物マネージャー
	std::unique_ptr<Stage> stage_;                 ///< ステージ（エリア・ウェーブ管理）
};

