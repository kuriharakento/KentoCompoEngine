#pragma once

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "scene/SceneContext.h"
#include "level/ILevelElement.h"
#include "level/Level.h"

namespace KCE
{
class DirectXCommon;
/**
 * @brief ステージの読み込みと破棄を受け持つ。どのシーンからでも、好きなときに使える。
 *
 * 使い方:
 *   Level* stage = LevelManager::GetInstance()->Load("live01");
 *   if (auto* ball = stage->Find<ModelElement>("mirror_ball")) { ... }
 *   LevelManager::GetInstance()->Unload("live01");
 *
 * - 初期化と更新はアプリ側（MyGame）が SceneContext を作ったあとに呼ぶ
 * - 同じ名前を2回 Load すると、読み込み済みのステージを返す
 */
class LevelManager
{
public:
	static LevelManager* GetInstance();
	static bool HasInstance();

	/**
	 * @brief 要素の種類を登録して初期化する
	 * @param context 要素を作るときに渡す、エンジン側の物への参照。LevelManager が終わるまで有効な前提
	 */
	void Initialize(const SceneContext& context);
	/** @brief 読み込み済みのステージをすべて破棄して、片付ける */
	void Finalize();

	/** @brief 要素の種類を登録する。登録順が読み込み順（参照する側を後に） */
	void RegisterElementType(LevelElementType type);
	const std::vector<LevelElementType>& GetElementTypes() const { return types_; }

	/**
	 * @brief ステージを読み込む。すでに読み込み済みならそれを返す
	 * @return 読み込めたステージ。所有は LevelManager。ファイルが壊れていれば nullptr
	 */
	Level* Load(const std::string& name);
	/** @brief ステージを破棄する。要素が持つ GameObject などもまとめて外れる */
	/**
	 * @brief Level を外す。実際に壊すのは次の ProcessPendingUnloads
	 * @details 描画の途中（ImGui のボタンなど）で GPU の物を壊すと、記録中のコマンドが壊れた物を指してエラーになるため
	 */
	void Unload(const std::string& name);
	/** @brief 外す予約をした Level を、GPU の描画が終わってから壊す。フレームの頭（更新）で呼ぶ */
	void ProcessPendingUnloads(DirectXCommon* dxCommon);
	/** @return 読み込み済みのステージ。無ければ nullptr */
	Level* Find(const std::string& name) const;

	/** @brief 読み込み済みの全ステージを更新する。毎フレーム呼ぶ */
	void Update();
	/** @brief シーケンサの状態。SequencerEditor が毎フレーム伝え、次の Update で要素へ渡す（active は対象を動かしている最中、playing は再生中） */
	void SetSequencerState(bool active, bool playing) { sequencerActive_ = active; sequencerPlaying_ = playing; }

	~LevelManager() = default;

private:
	static std::unique_ptr<LevelManager> instance_;
	friend std::unique_ptr<LevelManager> std::make_unique<LevelManager>();

	LevelManager() = default;
	LevelManager(const LevelManager&) = delete;
	LevelManager& operator=(const LevelManager&) = delete;

#ifdef USE_IMGUI
	/** @brief 「Level」ウィンドウの中身。ステージの選択・読み込み・再生・保存・要素の一覧 */
	void DrawWindowImGui();
	/** @brief ステージの選択と、フォルダからの読み込み・新規作成を描く */
	void DrawStagePickerImGui();
	/** @brief 選んでいるステージの操作行（再生・保存・追加）と要素の一覧を描く */
	void DrawStageBodyImGui(Level& stage);
	/** @brief Resources/json/level の中の .json のステージ名（拡張子なし）を並べる */
	static std::vector<std::string> ListStageFiles();
	/** @brief Resources/json/prefab の中の .json のファイル名を並べる */
	static std::vector<std::string> ListPrefabFiles();
	/** @brief 同じ名前が無いよう、元の名前に数字を足す */
	static std::string MakeUniqueName(const Level& stage, const std::string& baseName);
	/**
	 * @brief 要素を複製して同じ Level に足す
	 * @details 保存する形（Serialize）に書き出して、同じ種類の create で作り直す。名前は「元の名前_2」のように重ならない物にする
	 * @return 足せたら真
	 */
	bool DuplicateElement(Level& stage, const std::string& name);
#endif

	SceneContext context_{};
	std::vector<std::string> pendingUnloads_;
	bool sequencerActive_ = false;
	bool sequencerPlaying_ = false;
	std::vector<LevelElementType> types_;
	// 名前順に並べて、ウィンドウの並びが安定するようにする
	std::map<std::string, std::unique_ptr<Level>> stages_;
#ifdef USE_IMGUI
	// Level ウィンドウで編集しているステージの名前。読み込み済みでなければ、先頭のものに合わせる
	std::string selectedStageName_;
	// 新規作成する名前の入力欄
	std::array<char, 128> newStageName_{};
	// 要素の一覧を絞り込む検索欄
	std::array<char, 128> elementFilter_{};
	// 読み込みに失敗した理由。次に読み込むまで出しておく
	std::string loadError_;
	// 名前を変えている要素と、入力欄・エラー
	std::string renameTarget_;
	std::array<char, 128> renameBuffer_{};
	std::string renameError_;
	bool renamePopupRequested_ = false;
#endif
};
} // namespace KCE
