#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "base/GraphicsTypes.h"

namespace KCE
{
class GlyphAtlas;
class Sprite;
class SpriteCommon;

/**
 * @brief 文字列の揃え方。基準点（SetPosition）に対して行をどこへ置くか
 */
enum class TextAlign
{
	Left,
	Center,
	Right,
};

/**
 * @brief GlyphAtlas の文字を Sprite で並べて、日本語の文字列を描く
 *
 * - 座標は Sprite と同じ 1920x1080 の仮想画面。基準点は1行目の上端
 * - 値が変わったときだけ並べ直す。毎フレーム同じ値を入れても重くならない
 * - 描画は 2D の共通設定を済ませた後に呼ぶこと
 */
class TextSprite
{
public:
	/** @brief SetVisibleCount で全部を出すときの値 */
	static constexpr size_t kShowAll = static_cast<size_t>(-1);

	TextSprite();
	~TextSprite();

	/**
	 * @brief 初期化
	 * @param spriteCommon スプライト共通部。所有しない
	 * @param atlas 文字のアトラス。所有しない（このインスタンスより長生きする前提）
	 */
	void Initialize(SpriteCommon* spriteCommon, const GlyphAtlas* atlas);

	/** @brief 表示する文字列（UTF-8）。改行は '\n' */
	void SetText(const std::string& utf8Text);
	void SetPosition(const Vector2& position);
	/** @brief 画面上の文字の大きさ（ピクセル） */
	void SetFontSize(float pixelSize);
	void SetColor(const Vector4& color);
	void SetAlign(TextAlign align);
	/** @brief この幅を超えたら文字単位で折り返す。0 で折り返さない */
	void SetMaxWidth(float width);
	/** @brief 先頭から何文字まで出すか（タイプライター送り用） */
	void SetVisibleCount(size_t count);

	/** @brief 文字数（改行を含む UTF-16 の数） */
	size_t GetCharCount() const { return text_.size(); }

	void Draw();

private:
	struct Line
	{
		size_t begin = 0;
		size_t end = 0;
		float width = 0.0f;
	};

	/** @brief 文字ごとの Sprite を並べ直す */
	void Layout();
	/** @brief 使える Sprite を1つ取り出す。足りなければ作る */
	Sprite* AcquireSprite();

	// 所有しない
	SpriteCommon* spriteCommon_ = nullptr;
	const GlyphAtlas* atlas_ = nullptr;

	std::string utf8Text_;
	std::wstring text_;
	Vector2 position_{ 0.0f, 0.0f };
	float fontSize_ = 48.0f;
	Vector4 color_{ 1.0f, 1.0f, 1.0f, 1.0f };
	TextAlign align_ = TextAlign::Left;
	float maxWidth_ = 0.0f;
	size_t visibleCount_ = kShowAll;

	// 文字ごとの Sprite。減らさずに使い回す
	std::vector<std::unique_ptr<Sprite>> sprites_;
	size_t activeCount_ = 0;
	std::vector<Line> lines_;
	bool dirty_ = true;
};
} // namespace KCE
