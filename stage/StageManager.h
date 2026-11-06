#pragma once

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "scene/SceneContext.h"
#include "stage/IStageElement.h"
#include "stage/Stage.h"

namespace KCE
{
/**
 * @brief ステージの読み込みと破棄を受け持つ。どのシーンからでも、好きなときに使える。
 *
 * 使い方:
 *   Stage* stage = StageManager::GetInstance()->Load("live01");
 *   if (auto* ball = stage->Find<ModelElement>("mirror_ball")) { ... }
 *   StageManager::GetInstance()->Unload("live01");
 *
 * - 初期化と更新はアプリ側（MyGame）が SceneContext を作ったあとに呼ぶ
 * - 同じ名前を2回 Load すると、読み込み済みのステージを返す
 */
class StageManager
{
public:
	static StageManager* GetInstance();
	static bool HasInstance();

	/**
	 * @brief 要素の種類を登録して初期化する
	 * @param context 要素を作るときに渡す、エンジン側の物への参照。StageManager が終わるまで有効な前提
	 */
	void Initialize(const SceneContext& context);
	/** @brief 読み込み済みのステージをすべて破棄して、片付ける */
	void Finalize();

	/** @brief 要素の種類を登録する。登録順が読み込み順（参照する側を後に） */
	void RegisterElementType(StageElementType type);
	const std::vector<StageElementType>& GetElementTypes() const { return types_; }

	/**
	 * @brief ステージを読み込む。すでに読み込み済みならそれを返す
	 * @return 読み込めたステージ。所有は StageManager。ファイルが壊れていれば nullptr
	 */
	Stage* Load(const std::string& name);
	/** @brief ステージを破棄する。要素が持つ GameObject などもまとめて外れる */
	void Unload(const std::string& name);
	/** @return 読み込み済みのステージ。無ければ nullptr */
	Stage* Find(const std::string& name) const;

	/** @brief 読み込み済みの全ステージを更新する。毎フレーム呼ぶ */
	void Update();

	~StageManager() = default;

private:
	static std::unique_ptr<StageManager> instance_;
	friend std::unique_ptr<StageManager> std::make_unique<StageManager>();

	StageManager() = default;
	StageManager(const StageManager&) = delete;
	StageManager& operator=(const StageManager&) = delete;

#ifdef USE_IMGUI
	/** @brief 「Stage」ウィンドウの中身。ステージの選択・読み込み・再生・保存・要素の一覧 */
	void DrawWindowImGui();
	/** @brief ステージの選択と、フォルダからの読み込み・新規作成を描く */
	void DrawStagePickerImGui();
	/** @brief 選んでいるステージの操作行（再生・保存・追加）と要素の一覧を描く */
	void DrawStageBodyImGui(Stage& stage);
	/** @brief Resources/json/stage の中の .json のステージ名（拡張子なし）を並べる */
	static std::vector<std::string> ListStageFiles();
	/** @brief Resources/json/prefab の中の .json のファイル名を並べる */
	static std::vector<std::string> ListPrefabFiles();
	/** @brief 同じ名前が無いよう、元の名前に数字を足す */
	static std::string MakeUniqueName(const Stage& stage, const std::string& baseName);
#endif

	SceneContext context_{};
	std::vector<StageElementType> types_;
	// 名前順に並べて、ウィンドウの並びが安定するようにする
	std::map<std::string, std::unique_ptr<Stage>> stages_;
#ifdef USE_IMGUI
	// Stage ウィンドウで編集しているステージの名前。読み込み済みでなければ、先頭のものに合わせる
	std::string selectedStageName_;
	// 新規作成する名前の入力欄
	std::array<char, 128> newStageName_{};
	// 要素の一覧を絞り込む検索欄
	std::array<char, 128> elementFilter_{};
	// 読み込みに失敗した理由。次に読み込むまで出しておく
	std::string loadError_;
#endif
};
} // namespace KCE
