#pragma once
#include <array>
#include <string>
#include <vector>

#include "graphics/2d/SpriteAnimationClip.h"

namespace KCE
{
/**
 * @brief スプライトアニメーションの編集ウィンドウ
 *
 * @details Unity のアニメーションタブのように、画像1枚ごとに置く時刻と順番を変えられる。
 *          タイムライン上のキーの位置に、そのキーの画像をサムネイルで出す。
 *          画像1〜最後の画像までが1ループで、総秒数は最後のキーの時刻で決まる。
 *          ウィンドウは DebugUIManager に登録するので、描画の呼び出しは要らない。
 *          プレビューは ImGui の画像で出す（Sprite は使わない）。
 */
class SpriteAnimationEditor
{
public:
	~SpriteAnimationEditor();

	/**
	 * @brief ウィンドウを DebugUIManager に登録する
	 * @details TextureManager と DebugUIManager の初期化後に呼ぶ。
	 */
	void Initialize();

	/**
	 * @brief 編集中のクリップを返す
	 * @return 編集中のクリップ。ゲーム側の SpriteAnimator::SetClip に渡せる
	 */
	const SpriteAnimationClip& GetClip() const { return clip_; }

private:
#ifdef USE_IMGUI
	/** @brief ウィンドウの中身を描く */
	void DrawImGui();
	/** @brief 再生・停止・保存などの上の段 */
	void DrawToolbar();
	/** @brief 現在時刻の画像のプレビュー */
	void DrawPreview();
	/** @brief ルーラー・キー・サムネイル・再生ヘッドのタイムライン */
	void DrawTimeline();
	/** @brief 選んだキーの時刻と画像の編集欄 */
	void DrawKeyInspector();
	/** @brief 画像を選ぶポップアップ。選んだら outPath に入れて真を返す */
	bool DrawTexturePicker(const char* popupId, std::string& outPath);

	/** @brief キーを追加して選択する。画像が読めなければ追加しない */
	void AddKeyAt(float time);
	/** @brief 再生時刻を1フレーム進める */
	void AdvancePlayback();
	/** @brief 保存先のパスを返す */
	std::string BuildFilePath() const;
	/** @brief 名前欄の名前で保存する */
	void Save();
	/** @brief 名前欄の名前で読み込む */
	void Load();
	/** @brief 画像フォルダを調べ直して、選べる画像の一覧を作る */
	void RefreshTextureList();
	/** @brief 画像を読み込んでおく（読み込み済みなら何もしない） */
	void EnsureTextureLoaded(const std::string& path) const;
	/**
	 * @brief ImGui に渡す画像IDを返す
	 * @return 読み込み済みなら画像ID。未読み込みなら 0
	 */
	unsigned long long FindImGuiTexture(const std::string& path) const;
	/** @brief 今の再生時刻で出すキーの番号を返す。キーが無ければ -1 */
	int FindDisplayedKey() const;
#endif

private:
	// 編集中のクリップ
	SpriteAnimationClip clip_;
	// 再生時刻（秒）。停止中は総秒数より先にも置ける（キーを足す位置を決めるため）
	float time_ = 0.0f;
	// 再生速度の倍率
	float speed_ = 1.0f;
	// 再生中か
	bool isPlaying_ = false;
	// 選択中のキー。無ければ -1
	int selectedKey_ = -1;
	// ドラッグ中のキーを掴んでいるか
	bool isDraggingKey_ = false;
	// キーを掴んだ位置と、キーの時刻とのずれ（秒）。掴んだ瞬間にキーが跳ばないようにする
	float dragOffsetSeconds_ = 0.0f;
	// タイムラインの拡大率（1秒あたりのピクセル数）
	float pixelsPerSecond_ = 160.0f;
	// 次に足すキーの画像
	std::string pendingTexture_;
	// 保存名（拡張子なし）
	std::array<char, 64> nameBuffer_{};
	// 選べる画像の一覧（アプリのリソースの textures 以下からの相対パス）
	std::vector<std::string> textureList_;
	// 下の行に出す結果のメッセージ
	std::string statusMessage_;
};
} // namespace KCE
