#pragma once
#include <map>
#include <string>
#include <memory>
#include <unordered_set>
#include <vector>

// system
#include "graphics/3d/Model.h"
#include "graphics/3d/ModelCommon.h"
#include "manager/graphics/ResourceLifetime.h"

namespace KCE
{
class JobSystem;

/**
 * @brief モデルマネージャークラス
 * @details 3Dモデルのロードとキャッシングを管理するシングルトンクラス
 *          一度ロードしたモデルはキャッシュされ、再利用される
 */
class ModelManager
{
public: /*========[ 型 ]========*/
	/**
	 * @brief LoadModels でまとめて読むモデル1つ分
	 */
	struct ModelRequest
	{
		// モデルファイルパス（Resources/modelsディレクトリからの相対パス）
		std::string filePath;
		// モデルのファイル形式
		std::string modelType = ".obj";
	};

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
	void LoadModel(const std::string& filePath, const std::string& modelType = ".obj", ResourceLifetime lifetime = ResourceLifetime::Scene);

	/**
	 * @brief モデルをまとめて読む。ファイルの解析と、モデルが使うテクスチャの展開をワーカーで並べて回す
	 * @details GPU リソースは並べた順にメインスレッドで作る。メインスレッドから呼ぶこと。
	 * @param requests 読むモデル。読み込み済みと重複は飛ばす
	 * @param lifetime 寿命
	 * @param jobSystem ワーカー。nullptr なら順番に読む（所有しない）
	 */
	void LoadModels(const std::vector<ModelRequest>& requests, ResourceLifetime lifetime, JobSystem* jobSystem);

	/** @brief シーン寿命のモデルをまとめて解放する */
	void ReleaseSceneResources();

	/** @brief 常駐モデル数を取得する */
	size_t GetResidentModelCount() const;
	/** @brief シーン寿命のモデル数を取得する */
	size_t GetSceneModelCount() const;

	/**
	 * @brief 読み込み済みのモデルの名前（ファイルパス）を、名前順に返す
	 * @details エディタで GameObject のモデルを選ぶ一覧に使う。まだ読み込んでいないモデルは含まない
	 */
	std::vector<std::string> GetLoadedModelNames() const;

	/** @brief 読み込み済みのモデル1つ。エディタの一覧に使う */
	struct LoadedModel
	{
		std::string name;
		//! 読み込んだときのファイル形式（".obj" など）。不明なら空
		std::string type;
		ResourceLifetime lifetime = ResourceLifetime::Scene;
		//! 起動後に、名前で引かれた回数（物体にモデルを設定した回数など）
		uint64_t referenceCount = 0;
		//! 最後に引かれたフレーム。一度も引かれていなければ 0
		uint64_t lastReferenceFrame = 0;
	};
	/** @brief 読み込み済みのモデルを、名前順に返す */
	std::vector<LoadedModel> GetLoadedModels() const;
	/**
	 * @brief モデルを1つ解放する
	 * @details GPU がまだ使っているかもしれないので、呼ぶ前に GPU の完了を待つこと。
	 *          物体は、モデルを複製して持つので、元を解放しても、すでにある物体は動く。
	 *          ただし、名前で引き直すと、シーン寿命で読み直される
	 * @return 解放できたら真
	 */
	bool ReleaseModel(const std::string& name);
	/** @brief 読み込み済みか。FindModel と違い、読み直さず、参照の回数にも数えない */
	bool HasModel(const std::string& name) const { return models_.contains(name); }

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
	friend std::unique_ptr<ModelManager> std::make_unique<ModelManager>();

	// コピー禁止
	ModelManager()=default;

	ModelManager(const ModelManager&) = delete;
	ModelManager& operator=(const ModelManager&) = delete;

private: /*========[ 内部処理 ]========*/
	/**
	 * @brief 読み込み済みか、前に失敗したモデルかを調べる
	 * @details 読み込み済みのモデルを常駐で読もうとしたら、常駐へ昇格もする
	 * @return 読まなくていいなら true
	 */
	bool IsAlreadyHandled(const std::string& filePath, const std::string& modelType, ResourceLifetime lifetime);

	/**
	 * @brief 初期化できたモデルを登録する。常駐ならマテリアルのテクスチャも常駐にする
	 */
	void RegisterModel(const std::string& filePath, const std::string& modelType, std::unique_ptr<Model> model, ResourceLifetime lifetime);

private: /*========[ メンバ変数 ]========*/
	// モデルを探し始める場所
	static constexpr const char* kModelDirectory = "Resources/models";

	// モデル共通設定
	std::unique_ptr<ModelCommon> modelCommon_ = nullptr;


	// モデルデータのキャッシュ（ファイルパス -> モデル）
	struct ModelEntry
	{
		std::unique_ptr<Model> model;
		ResourceLifetime lifetime = ResourceLifetime::Scene;
		uint64_t referenceCount = 0;
		uint64_t lastReferenceFrame = 0;
	};
	std::map<std::string, ModelEntry> models_;
	std::map<std::string, std::string> knownModelTypes_;
	// 壊れたモデルを毎フレーム読み直して同じログを繰り返さないために記録する。
	std::unordered_set<std::string> failedModels_;
};
} // namespace KCE
