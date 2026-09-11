#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

namespace KCE
{
/**
 * @brief アトラス内の1文字の位置と送り幅（ピクセル）
 */
struct GlyphInfo
{
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
	// 次の文字までの幅
	float advance = 0.0f;
};

/**
 * @brief Windows のフォントから文字を1枚のテクスチャに焼いて持つ
 *
 * - ASCII・記号・かな・JIS 第1水準の漢字を入れる。第2水準の漢字は入らない（'?' で出る）
 * - テクスチャは TextureManager に kTextureKey で登録する
 * - Build は初期化中（GPU の完了待ちより前）に呼ぶこと。転送をそのフレームのコマンドに積むため
 */
class GlyphAtlas
{
public:
	/** @brief TextureManager に登録するときの名前 */
	static constexpr const char* kTextureKey = "runtime/glyph_atlas";

	/**
	 * @brief 文字を焼いてテクスチャを登録する
	 * @param fontPixelSize 焼くときの文字の大きさ（ピクセル）
	 * @return 焼けたら真。フォントが見つからない等で失敗したら偽
	 */
	bool Build(uint32_t fontPixelSize);

	/**
	 * @brief 文字を探す
	 * @param c UTF-16 の1文字
	 * @return 見つからなければ nullptr
	 */
	const GlyphInfo* Find(wchar_t c) const;

	bool IsReady() const { return ready_; }
	float GetFontPixelSize() const { return fontPixelSize_; }
	float GetLineHeight() const { return lineHeight_; }

private:
	std::unordered_map<wchar_t, GlyphInfo> glyphs_;
	float fontPixelSize_ = 0.0f;
	float lineHeight_ = 0.0f;
	bool ready_ = false;
};
} // namespace KCE
