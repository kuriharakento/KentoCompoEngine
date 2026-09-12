#pragma once
#include <cstddef>
#include <memory>
#include <string>

#include "graphics/2d/TextSprite.h"

namespace KCE
{
class GlyphAtlas;
class Sprite;
class SpriteCommon;

/**
 * @brief 画面の一番上に重ねる歌詞テロップと会話枠
 *
 * - シーケンサの Text トラックが毎フレーム Show / Hide で中身を決める
 * - 描画は 2D パスの最後（シーンの 2D より手前）
 * - 歌詞は画面下の中央、会話は画面下の枠に話者名と本文を出す
 */
class TextOverlay
{
public:
	TextOverlay();
	~TextOverlay();

	/**
	 * @brief 初期化。初期化中（GPU の完了待ちより前）に呼ぶこと
	 * @param spriteCommon スプライト共通部（会話枠を描く）。所有しない
	 * @param atlas 文字のアトラス。所有しない
	 * @param textPipeline 文字の描き方。所有しない
	 */
	void Initialize(SpriteCommon* spriteCommon, GlyphAtlas* atlas, TextSpritePipeline* textPipeline);

	/**
	 * @brief 歌詞を出す
	 * @param text 歌詞（UTF-8）
	 * @param alpha 不透明度。0 以下なら出さない
	 */
	void ShowLyric(const std::string& text, float alpha);
	void HideLyric();

	/**
	 * @brief 会話を出す
	 * @param speaker 話者名（UTF-8）。空なら名前を出さない
	 * @param text 本文（UTF-8）
	 * @param visibleCount 先頭から出す文字数（タイプライター送り）
	 * @param alpha 不透明度。0 以下なら出さない
	 */
	void ShowDialogue(const std::string& speaker, const std::string& text, size_t visibleCount, float alpha);
	void HideDialogue();

	void Draw();

private:
	TextSprite lyric_;
	// 明るい背景でも読めるように、ずらした影を下に敷く
	TextSprite lyricShadow_;
	TextSprite speaker_;
	TextSprite body_;
	std::unique_ptr<Sprite> dialogueBox_;
	// 文字を描いた後に会話枠（Sprite）を描くとき、Sprite 用の設定に戻すために使う。所有しない
	SpriteCommon* spriteCommon_ = nullptr;
	bool lyricVisible_ = false;
	bool dialogueVisible_ = false;
};
} // namespace KCE
