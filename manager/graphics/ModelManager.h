#pragma once
#include <map>
#include <string>
#include <memory>

// system
#include "graphics/3d/Model.h"
#include "graphics/3d/ModelCommon.h"

namespace KCE
{
/**
 * @brief モデルマネージャークラス
 * @details 3Dモデルのロードとキャッシングを管理するシングルトンクラス
 *          一度ロードしたモデルはキャッシュされ、再利用される
 */
class ModelManager
{
public: /*========[ メンバ関数 ]========*/
	/**
	 * @brief シングルトンインスタンスを取得
	 * @return ModelManagerのインスタンス
	 */
	static ModelManager* GetInstance();

	/**
	 * @brief 初期化処理
	 * @param dxCommon DirectXCommonへのポインタ
	 */
	void Initialize(DirectXCommon* dxCommon);

	/**
	 * @brief 終了処理
	 * @details リソースを解放し、インスタンスを削除する
	 */
	void Finalize();

	/**
	 * @brief モデルの読み込み
	 * @param filePath モデルファイルパス（Resources/modelsディレクトリからの相対パス）
	 * @param modelType モデルのファイル形式（デフォルト: ".obj"）
	 * @details 読み込み済みの場合はスキップされる
	 */
	void LoadModel(const std::string& filePath, const std::string& modelType = ".obj");

	/**
	 * @brief モデルの検索
	 * @param filePath モデルファイルパス
	 * @return モデルへのポインタ（見つからない場合はnullptr）
	 */
	Model* FindModel(const std::string& filePath);

	/**
	 * @brief ModelCommonの取得
	 * @return ModelCommonへのポインタ
	 */
	ModelCommon* GetModelCommon() const { return modelCommon_.get(); }

public:
	~ModelManager()=default;

private: /*========[ シングルトン ]========*/
	static std::unique_ptr<ModelManager> instance_; // シングルトンインスタンス
	
	// コピー禁止
	ModelManager()=default;
	
	ModelManager(const ModelManager&) = delete;
	ModelManager& operator=(const ModelManager&) = delete;

private: /*========[ メンバ変数 ]========*/
	// モデル共通設定
	std::unique_ptr<ModelCommon> modelCommon_ = nullptr;


	// モデルデータのキャッシュ（ファイルパス -> モデル）
	std::map<std::string, std::unique_ptr<Model>> models_;
};
} // namespace KCE
