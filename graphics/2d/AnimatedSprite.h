#pragma once
#include <string>

#include "graphics/2d/Sprite.h"
#include "graphics/2d/SpriteAnimationClip.h"
#include "graphics/2d/SpriteAnimator.h"

namespace KCE
{
class SpriteCommon;

/**
 * @brief アニメーション付きスプライト
 *
 * @details Sprite・SpriteAnimator・クリップの読み込みをひとまとめにしたもの。
 *          エディタで保存したクリップの名前を渡して Initialize し、
 *          毎フレーム Update(dt) と Draw() を呼ぶだけで動く。
 *          位置・色・回転などは GetSprite() から Sprite をそのまま触る。
 *          画像ごとに大きさが違っても表示サイズは保つ（SetKeepSize(false) で画像に合わせる）。
 *
 * @code
 * AnimatedSprite sprite;
 * sprite.Initialize(spriteCommon, "NewSpriteAnimation");
 * sprite.GetSprite().SetPosition({ 100.0f, 100.0f });
 * // 毎フレーム
 * sprite.Update(dt);
 * sprite.Draw();
 * @endcode
 */
class AnimatedSprite
{
public:
	/**
	 * @brief 保存名からクリップを読んで初期化する
	 * @details クリップ内の画像も読み込む（読み込み済みなら何もしない）。
	 * @param spriteCommon スプライト共通部。所有しない
	 * @param clipName エディタで保存した名前（拡張子なし）
	 * @return 初期化できたら真。クリップが読めない・キーが無いときは偽（Update/Draw は何もしない）
	 */
	bool Initialize(SpriteCommon* spriteCommon, const std::string& clipName);

	/**
	 * @brief クリップを直接渡して初期化する
	 * @param spriteCommon スプライト共通部。所有しない
	 * @param clip 再生するクリップ（コピーされる）
	 * @return 初期化できたら真。キーが無ければ偽
	 */
	bool Initialize(SpriteCommon* spriteCommon, const SpriteAnimationClip& clip);

	/**
	 * @brief 1フレーム分進めて、Sprite の頂点と行列まで更新する
	 * @param dt 経過秒
	 */
	void Update(float dt);

	/**
	 * @brief 描画する
	 */
	void Draw();

	/** @brief 再生を始める（時刻は保ったまま） */
	void Play() { animator_.Play(); }
	/** @brief 一時停止する */
	void Pause() { animator_.Pause(); }
	/** @brief 停止して先頭の画像に戻す */
	void Stop() { animator_.Stop(); }

	/**
	 * @brief 表示サイズを設定する
	 * @details 保つ設定のときは、画像が切り替わってもこの大きさのまま。
	 * @param size 幅・高さ
	 */
	void SetSize(const KCE::Vector2& size);

	/**
	 * @brief 画像が切り替わったとき、表示サイズを保つか
	 * @param keepSize 真なら SetSize の大きさのまま。偽なら画像の大きさに合わせる
	 */
	void SetKeepSize(bool keepSize) { keepSize_ = keepSize; }

	/**
	 * @brief 再生速度の倍率を設定する
	 * @param speed 1.0 が等倍
	 */
	void SetSpeed(float speed) { animator_.SetSpeed(speed); }

	/**
	 * @brief ループするかを設定する
	 * @details クリップ側の設定を上書きする。
	 * @param isLooping ループするか
	 */
	void SetLooping(bool isLooping);

	bool IsInitialized() const { return isInitialized_; }
	bool IsPlaying() const { return animator_.IsPlaying(); }
	float GetTime() const { return animator_.GetTime(); }
	float GetDuration() const { return animator_.GetClip().GetDuration(); }
	Sprite& GetSprite() { return sprite_; }
	const Sprite& GetSprite() const { return sprite_; }

private:
	Sprite sprite_;
	SpriteAnimator animator_;
	// 保つ設定のときの表示サイズ
	KCE::Vector2 fixedSize_ = { 0.0f, 0.0f };
	bool keepSize_ = true;
	bool isInitialized_ = false;
};
} // namespace KCE
