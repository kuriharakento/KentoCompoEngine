#pragma once
#include <array>
#include <memory>
#include <string>
#include <vector>

#include "graphics/2d/SpriteAnimationClip.h"

namespace KCE
{
class Sprite;
class SpriteCommon;

/**
 * @brief スプライトアニメーションの編集ウィンドウ
 *
 * @details Unity のアニメーションタブのように、画像1枚ごとに置く時刻と順番を変えられる。
 *          タイムライン上のキーの位置に、そのキーの画像をサムネイルで出す。
 *          画像1〜最後の画像までが1ループで、総秒数は最後のキーの時刻で決まる。
 *          ウィンドウは DebugUIManager に登録するので、描画の呼び出しは要らない。
 *          プレビューは2つある。エディタ内の小さな枠（ImGui の画像）と、
 *          「ゲーム画面に表示」を入れたときにゲーム画面の 2D 描画で実際に出す Sprite。
 */
class SpriteAnimationEditor
{
public:
	// Sprite を前方宣言のまま unique_ptr で持つので、生成と破棄は cpp 側に置く
	SpriteAnimationEditor();
	~SpriteAnimationEditor();

	/**
	 * @brief ウィンドウを DebugUIManager に登録する
	 * @details TextureManager と DebugUIManager の初期化後に呼ぶ。
	 * @param spriteCommon 画面表示のプレビューに使う共通部。所有しない（Framework が先に壊さない）
	 */
	void Initialize(SpriteCommon* spriteCommon);

	/**
	 * @brief 「ゲーム画面に表示」がオンなら、今の時刻の画像を実際のスプライトとして描く
	 * @details 2D 描画パスの中（スプライト共通設定の後）から呼ぶ。オフなら何もしない。
	 */
	void DrawScenePreview();

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
	// 画面表示のプレビューに使う共通部。所有しない（Framework が保持し、エディタより後に壊れる）
	SpriteCommon* spriteCommon_ = nullptr;
	// 画面表示のプレビュー用スプライト。最初に描くときに作る
	std::unique_ptr<Sprite> previewSprite_;
	// 今 previewSprite_ に貼っている画像。変わったときだけ SetTexture する
	std::string previewTexturePath_;
	// ゲーム画面に出すか
	bool showOnScreen_ = false;
	// 画面表示の中心位置（Sprite の 1920x1080 座標）
	std::array<float, 2> previewPosition_ = { 960.0f, 540.0f };
	// 画面表示の拡大率（画像の元の大きさに対する倍率）
	float previewScale_ = 1.0f;

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
	// タイムラインで何を掴んでいるか
	enum class DragMode
	{
		None,		//!< 何も掴んでいない（再生ヘッドを動かす）
		MoveKey,	//!< キーの時刻を動かす
		ResizeKey,	//!< キーの右端を動かして表示時間を変える
	};
	DragMode dragMode_ = DragMode::None;
	// 掴んだ位置と、掴んだ場所（キーの時刻か右端）とのずれ（秒）。掴んだ瞬間に跳ばないようにする
	float dragOffsetSeconds_ = 0.0f;
	// タイムラインの拡大率（1秒あたりのピクセル数）
	float pixelsPerSecond_ = 160.0f;
	// 次に足すキーの画像
	std::string pendingTexture_;
	// 次に足すキーの表示時間（秒）
	float newKeyDuration_ = kSpriteAnimationDefaultKeyDuration;
	// 保存名（拡張子なし）
	std::array<char, 64> nameBuffer_{};
	// 選べる画像の一覧（アプリのリソースの textures 以下からの相対パス）
	std::vector<std::string> textureList_;
	// 下の行に出す結果のメッセージ
	std::string statusMessage_;
};
} // namespace KCE
