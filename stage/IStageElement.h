#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

namespace KCE
{
struct SceneContext;
#ifdef USE_IMGUI
struct SelectionItem;
struct GizmoResult;
struct Matrix4x4;
#endif

/**
 * @brief ステージに置く物1つ。3D 文字、モニター、カメラ、モデルなど種類ごとに継承する。
 *
 * - 自分が使うエンジン側の物（TextMesh3D、GameObject、RenderView など）を所有し、破棄のときに登録を外す
 * - 保存は Serialize、読み込みは StageElementType::create が受け持つ。Stage は中身の種類を知らない
 * - 名前はステージ内で重複しない。要素どうしの参照（モニターが使うカメラなど）も名前で指す
 */
class IStageElement
{
public:
	virtual ~IStageElement() = default;

	/** @return ステージ内で一意な名前 */
	const std::string& GetName() const { return name_; }

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
	explicit IStageElement(std::string name) : name_(std::move(name)) {}

private:
	std::string name_;
};

/**
 * @brief 要素の種類1つぶんの登録。JSON の配列名と、そこから要素を作る関数をまとめる
 *
 * 登録した順が、読み込みの順になる。ほかの要素を参照する種類（モニター）は、参照される種類（カメラ）より後に登録する
 */
struct StageElementType
{
	//! JSON の配列名。IStageElement::GetTypeKey と同じ
	std::string key;
	//! エディタの「追加」に出す名前
	std::string displayName;
	//! JSON の1件から要素を作る。不正なら nullptr（呼び出し側が読み飛ばす）
	std::function<std::unique_ptr<IStageElement>(const nlohmann::json& entry, const SceneContext& context)> create;
	//! エディタで新規に作るときの初期状態。エディタが無いビルドでは空でよい
	std::function<std::unique_ptr<IStageElement>(const std::string& name, const SceneContext& context)> createDefault;
};
} // namespace KCE
