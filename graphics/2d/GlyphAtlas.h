#pragma once
#include <cstdint>
#include <memory>

namespace KCE
{
class DirectXCommon;

/**
 * @brief アトラス内の1文字の位置と並べ方（ピクセル）
 */
struct GlyphInfo
{
	// アトラス内の絵の左上と大きさ。空白など絵の無い文字は 0
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
	// ベースラインの原点から絵の左上までのずれ
	float offsetX = 0.0f;
	float offsetY = 0.0f;
	// 次の文字までの幅
	float advance = 0.0f;
};

/**
 * @brief DirectWrite で文字を描いて、1枚のテクスチャに詰めて持つ（動的アトラス）
 *
 * - 起動時は ASCII・かな・記号だけ描いておき、漢字などは初めて使われたときに描き足す
 * - 日本語フォントに無い文字は、候補のフォントを順に当たって見つかったもので描く
 * - 描き足した部分は FlushUploads で GPU へ送る。描画コマンドの途中で呼んでよい
 * - テクスチャは TextureManager に kTextureKey で登録する
 */
class GlyphAtlas
{
public:
	/** @brief TextureManager に登録するときの名前 */
	static constexpr const char* kTextureKey = "runtime/glyph_atlas";

	GlyphAtlas();
	~GlyphAtlas();

	/**
	 * @brief フォントを用意して、よく使う文字を先に描いておく
	 * @details 初期化中（GPU の完了待ちより前）に呼ぶこと。
	 * @param dxCommon テクスチャの作成と転送に使う。所有しない（このインスタンスより長生きする前提）
	 * @param fontPixelSize 描くときの文字の大きさ（ピクセル）
	 * @return 用意できたら真。日本語フォントが見つからない等で失敗したら偽
	 */
	bool Build(DirectXCommon* dxCommon, uint32_t fontPixelSize);

	/**
	 * @brief 文字を取り出す。まだ無ければその場で描き足す
	 * @param c UTF-16 の1文字
	 * @return どのフォントにも無い、またはアトラスが一杯なら nullptr。
	 *         返したポインタはこのインスタンスが生きている間ずっと有効
	 */
	const GlyphInfo* Acquire(wchar_t c);

	/** @brief 描き足した部分を GPU へ送るコマンドを積む。何も無ければ何もしない */
	void FlushUploads();

	/**
	 * @brief フレームの頭で呼ぶ。前のフレームの転送に使ったバッファを捨てる
	 * @details フレームの終わりで GPU の完了を待っている前提。
	 */
	void BeginFrame();

	bool IsReady() const { return ready_; }
	float GetFontPixelSize() const { return fontPixelSize_; }
	float GetLineHeight() const { return lineHeight_; }
	// 行の上端からベースラインまで
	float GetAscent() const { return ascent_; }

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
	float fontPixelSize_ = 0.0f;
	float lineHeight_ = 0.0f;
	float ascent_ = 0.0f;
	bool ready_ = false;
};
} // namespace KCE
