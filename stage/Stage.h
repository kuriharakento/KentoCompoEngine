#pragma once

#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "stage/IStageElement.h"

namespace KCE
{
struct SceneContext;

/**
 * @brief ステージ1つぶん。要素を所有し、JSON ファイルへの保存と読み込みを受け持つ。
 *
 * - ファイルは Resources/json/stage/<名前>.json。1ステージ1ファイル
 * - 種類ごとの配列を持つ（texts3d / cameras / monitors / models ...）。既存の feature_check.json をそのまま読める
 * - 作るのは StageManager だけ。Undo のコマンドは weak_ptr で持ち、ステージが破棄されたあとは何もしない
 */
class Stage : public std::enable_shared_from_this<Stage>
{
public:
	/** @param context 要素を作るときに渡す、エンジン側の物への参照。Stage より長生きする前提 */
	Stage(std::string name, const SceneContext& context);
	~Stage();

	const std::string& GetName() const { return name_; }

	/** @brief ファイルを読んで要素を作る。無ければ空のステージとして成功する */
	bool Load();
	/** @brief 今の内容をファイルへ保存する。一時ファイルへ書いてから置き換える */
	bool Save();

	/** @brief 全要素を更新する。StageManager が毎フレーム呼ぶ */
	void Update(float deltaTime);
	/** @brief 全要素へ再生・停止を伝える */
	void SetPlaying(bool playing);
	bool IsPlaying() const { return playing_; }

	/**
	 * @brief 要素を名前で探す
	 * @return 無い、または型が違えば nullptr
	 */
	template<class T>
	T* Find(const std::string& name) const
	{
		return dynamic_cast<T*>(FindElement(name));
	}

	/**
	 * @brief 要素を追加して所有する
	 * @param element 成功したときだけ所有権を受け取る。名前が重複していたら呼び出し側に残る
	 */
	bool Add(std::unique_ptr<IStageElement>& element);
	/** @brief 要素を外して所有権を返す。Undo のコマンドが持っておけるようにするため */
	std::unique_ptr<IStageElement> Remove(const std::string& name);

	const std::vector<std::unique_ptr<IStageElement>>& GetElements() const { return elements_; }

	/**
	 * @brief 保存したあとに内容が変わったか
	 * @details Serialize して保存時の JSON と比べるので、毎フレーム呼ばない。エディタの表示用に必要なときだけ呼ぶ
	 */
	bool IsDirty() const;

private:
	IStageElement* FindElement(const std::string& name) const;
	nlohmann::ordered_json Serialize() const;
	bool Deserialize(const nlohmann::json& json, std::string& outError);

	std::string name_;
	const SceneContext& context_;
	std::vector<std::unique_ptr<IStageElement>> elements_;
	// 登録された種類のキーでも version でもない、読み込んだ項目。保存で消さないよう、そのまま書き戻す
	// （まだ新しい形に移していない旧ステージの texts3d / cameras / monitors など）
	nlohmann::ordered_json passthrough_ = nlohmann::ordered_json::object();
	// 保存（か読み込み）した時点の JSON。IsDirty が今の内容と比べる
	nlohmann::ordered_json savedSnapshot_;
	bool playing_ = false;
};
} // namespace KCE
