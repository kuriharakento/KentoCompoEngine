#pragma once
#include <memory>

namespace KCE
{
/**
 * @brief エディタの動作モード
 */
enum class EditorMode
{
	Edit, //!< 編集中。ゲーム時間は止まり、絵はシーケンサの時刻だけで決まる
	Play, //!< 再生中。通常のゲームループが動く
};

/**
 * @brief 編集モードと再生モードを分離するクラス
 *
 * @details SEQUENCER_PLAN 5.1。従来は常にゲームが動いていたが、
 *          演出を編集するには「ゲーム時間を止め、シーケンサの時刻だけで絵を作る」
 *          状態が必要になる。
 *
 *          編集モードでは TimeManager をポーズするため、
 *          deltaTime に依存した更新はすべて止まる。この状態で正しい絵が出るかどうかが、
 *          Evaluate(t) の純関数契約（3.1）が守られているかの検証そのものになる。
 */
class EditorContext
{
public:
	static EditorContext* GetInstance();
	static bool HasInstance();

	/**
	 * @brief 終了処理
	 */
	void Finalize();

	/**
	 * @brief モードを切り替える
	 * @details 編集モードに入るとゲーム時間を止め、再生モードに戻すと再開する。
	 *          同じモードを指定した場合は何もしない。
	 * @param mode 切り替え先のモード
	 */
	void SetMode(EditorMode mode);

	/**
	 * @brief 編集モードと再生モードを切り替える
	 */
	void ToggleMode();

	EditorMode GetMode() const { return mode_; }
	bool IsEditMode() const { return mode_ == EditorMode::Edit; }
	bool IsPlayMode() const { return mode_ == EditorMode::Play; }

	/**
	 * @brief 編集モード中に1フレームだけゲームを進める要求を出す
	 * @details 物理やパーティクルの挙動を1フレームずつ確認するために使う。
	 */
	void RequestStepFrame() { stepFrameRequested_ = true; }

	/**
	 * @brief 1フレーム進める要求を取り出す
	 * @details 要求はこの呼び出しで消費される。フレームの更新処理から1回だけ呼ぶこと。
	 * @return 要求があれば真
	 */
	bool ConsumeStepFrameRequest();

	/**
	 * @brief この時点でゲームロジックを更新してよいか
	 * @details 再生モード中、または編集モードで1フレーム進める要求があるときに真。
	 * @return 更新してよければ真
	 */
	bool ShouldUpdateGameLogic() const;

public:
	~EditorContext() = default;

private:
	static std::unique_ptr<EditorContext> instance_;
	friend std::unique_ptr<EditorContext> std::make_unique<EditorContext>();

	EditorContext() = default;
	EditorContext(const EditorContext&) = delete;
	EditorContext& operator=(const EditorContext&) = delete;

	// 現在のモード。既定は従来通りゲームが動く再生モード
	EditorMode mode_ = EditorMode::Play;
	// 編集モード中の1フレーム進める要求
	bool stepFrameRequested_ = false;
};
} // namespace KCE
