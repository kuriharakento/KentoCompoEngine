#pragma once
#include <cstddef>
#include <string>
#include <vector>

#include "math/MatrixFunc.h"
#include "math/Quaternion.h"
#include "math/Vector3.h"
#include "math/Vector4.h"

namespace KCE
{
class GlyphAtlas;

/**
 * @brief 1文字ずつの出入りの動き方
 */
enum class TextAppearStyle
{
	Fade, //!< その場で透明度だけ変わる
	Drop, //!< 上から落ちてきて、下へ抜けていく
	Spin, //!< 縦軸で回りながら出入りする
	Pop,  //!< 小さい所から弾むように大きくなる
};

/** @brief 出入りの動き方を保存用の文字列へ変換する。 */
const char* TextAppearStyleToString(TextAppearStyle style);

/** @brief 保存用の文字列から出入りの動き方へ変換する。 */
TextAppearStyle TextAppearStyleFromString(const std::string& value);

/**
 * @brief 3D 空間に置く文字列（1文字ごとに板を並べる）
 *
 * - 位置・回転・大きさはワールドの値。size は1文字（1em）のワールドでの高さ
 * - 出入りは reveal / exit で決まる。先頭から数えて reveal 文字目までが出ていて、
 *   exit 文字目までが抜けている。小数の分は途中の動き。値だけで絵が決まるので
 *   シーケンサのスクラブやスキップでもそのまま正しく出る
 * - 描くのは Text3DRenderer。登録された名前で探される
 */
class TextMesh3D
{
public:
	/** @brief 全部出しておくときの reveal */
	static constexpr float kRevealAll = 1.0e6f;

	/** @brief Text3D.VS.hlsl の struct GlyphInstance と一致させること */
	struct Instance
	{
		Matrix4x4 world;
		// アトラスの UV（左上 xy、右下 zw）
		Vector4 uvRect;
		Vector4 color;
	};
	static_assert(sizeof(Instance) == 96, "GlyphInstance のサイズがシェーダー側と一致しません");

	struct Params
	{
		Vector3 position = { 0.0f, 0.0f, 0.0f };
		Quaternion rotation = Quaternion::Identity();
		// 全体の拡大率
		float scale = 1.0f;
		// 1文字（1em）のワールドでの高さ
		float size = 1.0f;
		// HDR 可。1 を超えるとブルームで光る
		Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
		float reveal = kRevealAll;
		float exit = 0.0f;
		TextAppearStyle style = TextAppearStyle::Fade;
	};

	/** @brief 表示する文字列（UTF-8）。改行は '\n' */
	void SetText(const std::string& utf8Text);
	const std::string& GetText() const { return utf8Text_; }

	Params& GetParams() { return params_; }
	const Params& GetParams() const { return params_; }

	/** @brief 出入りの対象になる文字数（改行を除く） */
	size_t GetCharCount() const { return charCount_; }

	/**
	 * @brief 文字ごとの板を作って out の後ろへ足す
	 * @details 毎フレーム呼ばれる。out は呼ぶ側が使い回す（ここでは確保しない）。
	 *          初めて使う文字はアトラスへ描き足される。
	 * @param atlas 文字のアトラス
	 * @param out 足し込む先
	 */
	void BuildInstances(GlyphAtlas& atlas, std::vector<Instance>& out) const;

private:
	std::string utf8Text_;
	std::wstring text_;
	size_t charCount_ = 0;
	Params params_;
};
} // namespace KCE
