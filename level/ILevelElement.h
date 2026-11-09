#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

namespace KCE
{
struct SceneContext;
class Level;
#ifdef USE_IMGUI
struct SelectionItem;
struct GizmoResult;
struct Matrix4x4;
#endif

/**
 * @brief ステージに置く物1つ。3D 文字、モニター、カメラ、モデルなど種類ごとに継承する。
 *
 * - 自分が使うエンジン側の物（TextMesh3D、GameObject、RenderView など）を所有し、破棄のときに登録を外す
 * - 保存は Serialize、読み込みは LevelElementType::create が受け持つ。Level は中身の種類を知らない
 * - 名前はステージ内で重複しない。要素どうしの参照（モニターが使うカメラなど）も名前で指す
 */
class ILevelElement
{
public:
	virtual ~ILevelElement() = default;

	/** @brief 所属するステージ。Level が追加のときに設定する。要素どうしの参照（モニターが使うカメラなど）に使う */
	void BindLevel(Level* stage)
	{
		level_ = stage;
		OnLevelBound();
	}
	Level* GetLevel() const { return level_; }

	/** @return ステージ内で一意な名前 */
	const std::string& GetName() const { return name_; }

	/**
	 * @brief 名前を変える前に呼ばれる。持っている物（登録名など）を新しい名前へ移す
	 * @return 移せなければ偽（名前は変わらない）
	 */
	virtual bool OnRename(const std::string& newName) { (void)newName; return true; }
	/** @brief 別の要素の名前が変わったときに呼ばれる。名前で参照しているなら、書き換える */
	virtual void OnElementRenamed(const std::string& oldName, const std::string& newName) { (void)oldName; (void)newName; }
	/** @brief 名前を確定する。Level::Rename だけが呼ぶ */
	void SetName(std::string name) { name_ = std::move(name); }

	/**
	 * @brief JSON の配列名。種類ごとに固定の文字列を返す
	 * @details 例: "texts3d" / "cameras" / "monitors" / "models"
	 */
	virtual const char* GetTypeKey() const = 0;

	/**
	 * @brief 毎フレームの更新。更新の要らない要素は何もしない
	 * @param deltaTime 経過時間（秒）
	 */
	virtual void Update(float deltaTime) { (void)deltaTime; }

	/**
	 * @brief エディタの再生・停止を伝える
	 * @details 再生で動き出す物（GameObject のコンポーネントなど）を持つ要素だけが実装する。
	 *          停止のときは再生前の姿勢へ戻す
	 */
	virtual void SetPlaying(bool playing) { (void)playing; }

	/**
	 * @brief シーケンサの状態を伝える（毎フレーム）
	 * @param active シーケンサが対象を動かしている最中か（再生中・一時停止・スクラブ）。偽の間は、いまの姿が置き場所
	 * @param playing シーケンサが再生中か。再生中は、コンポーネントも一緒に動かす
	 */
	virtual void OnSequencerState(bool active, bool playing) { (void)active; (void)playing; }

	/** @brief 今の状態を JSON にする。読み込みで同じ状態に戻せること */
	virtual nlohmann::ordered_json Serialize() const = 0;

#ifdef USE_IMGUI
	/** @brief Hierarchy に並べる1行の描画。選んだら SelectionContext へ伝える */
	virtual void DrawHierarchyRow() = 0;
	/** @brief 選択が自分の物なら Inspector の中身を描く。選択の種類が合わなければ何もしない */
	virtual void DrawInspector(const SelectionItem& selected) = 0;
	/**
	 * @brief 選択が自分の物なら、ギズモで動かす姿勢を返す。自分の物でなければ false
	 * @param operations 使える操作（GizmoOperationFlags）。既定は全部
	 */
	virtual bool GetGizmoPose(const SelectionItem& selected, Matrix4x4& world, uint32_t& operations) const
	{
		(void)selected; (void)world; (void)operations;
		return false;
	}
	/** @brief ギズモで動かした結果を受け取る。自分の物でなければ何もしない */
	virtual void ApplyGizmo(const SelectionItem& selected, const GizmoResult& result, uint32_t dragId)
	{
		(void)selected; (void)result; (void)dragId;
	}
#endif

protected:
	explicit ILevelElement(std::string name) : name_(std::move(name)) {}

	/** @brief 所属する Level が決まった。GameObject を持つ要素は、ここで名前を "Level_ファイル名_要素名" にする */
	virtual void OnLevelBound() {}
	/** @brief GameObject につける名前。他の Level や本編の物と名前がぶつからないよう、Level の名前を前に付ける */
	std::string MakeObjectName(const std::string& elementName) const;

private:
	std::string name_;
	// 所有しない。要素より長生きする
	Level* level_ = nullptr;
};

/**
 * @brief 要素の種類1つぶんの登録。JSON の配列名と、そこから要素を作る関数をまとめる
 *
 * 登録した順が、読み込みの順になる。ほかの要素を参照する種類（モニター）は、参照される種類（カメラ）より後に登録する
 */
struct LevelElementType
{
	//! JSON の配列名。ILevelElement::GetTypeKey と同じ
	std::string key;
	//! エディタの「追加」に出す名前
	std::string displayName;
	//! 新しく作る要素の名前の元（英数字。重複したら _2 などを足す）
	std::string defaultName;
	//! JSON の1件から要素を作る。不正なら nullptr（呼び出し側が読み飛ばす）
	std::function<std::unique_ptr<ILevelElement>(const nlohmann::json& entry, const SceneContext& context)> create;
	//! エディタで新規に作るときの初期状態。エディタが無いビルドでは空でよい
	std::function<std::unique_ptr<ILevelElement>(const std::string& name, const SceneContext& context)> createDefault;
};
} // namespace KCE
