#pragma once
#include "graphics/2d/SpriteAnimationClip.h"

namespace KCE
{
class Sprite;

/**
 * @brief SpriteAnimationClip を時間で進めて、Sprite のテクスチャを切り替える
 *
 * @details クリップは値で持つ（コピー）。Sprite は借りるだけ。
 *          画像が変わったフレームだけ SetTexture を呼ぶので、毎フレームの確保は無い。
 *          SetTexture はサイズもテクスチャ寸法に合わせ直すため、画像ごとに大きさが違えば
 *          表示サイズも追従する。
 */
class SpriteAnimator
{
public:
	/**
	 * @brief 対象のスプライトを設定する
	 * @param sprite 対象。所有しない（呼び出し側が Animator より長く生かす）。nullptr可
	 */
	void Initialize(Sprite* sprite);

	/**
	 * @brief クリップを設定して先頭に戻す
	 * @param clip 再生するクリップ（コピーされる）
	 */
	void SetClip(const SpriteAnimationClip& clip);

	/**
	 * @brief 再生を始める（時刻は保ったまま）
	 */
	void Play() { isPlaying_ = true; }

	/**
	 * @brief 一時停止する
	 */
	void Pause() { isPlaying_ = false; }

	/**
	 * @brief 停止して先頭の画像に戻す
	 */
	void Stop();

	/**
	 * @brief 1フレーム分進める
	 * @param dt 経過秒。負は0扱い
	 */
	void Update(float dt);

	/**
	 * @brief 再生時刻を直接指定して画像を反映する（エディタのスクラブ用）
	 * @param time 再生時刻（秒）
	 */
	void SetTime(float time);

	float GetTime() const { return time_; }
	bool IsPlaying() const { return isPlaying_; }
	/**
	 * @brief ループの設定を切り替える（再生時刻は保つ）
	 * @param isLooping ループするか
	 */
	void SetLooping(bool isLooping) { clip_.SetLooping(isLooping); }

	float GetSpeed() const { return speed_; }
	void SetSpeed(float speed) { speed_ = speed; }
	const SpriteAnimationClip& GetClip() const { return clip_; }

private:
	/**
	 * @brief 現在時刻のキーを Sprite に反映する。同じキーなら何もしない
	 */
	void Apply();

private:
	// 対象のスプライト。所有しない（呼び出し側が先に破棄しない前提）
	Sprite* sprite_ = nullptr;
	SpriteAnimationClip clip_;
	// 再生時刻（秒）
	float time_ = 0.0f;
	// 再生速度の倍率
	float speed_ = 1.0f;
	bool isPlaying_ = true;
	// 最後に反映したキー。まだ何も反映していなければ -1
	int appliedKeyIndex_ = -1;
};
} // namespace KCE
