#include "graphics/text/TextMesh3D.h"

#include <algorithm>
#include <numbers>

#include "base/StringUtility.h"
#include "graphics/2d/GlyphAtlas.h"

namespace KCE
{
const char* TextAppearStyleToString(TextAppearStyle style)
{
	switch (style)
	{
	case TextAppearStyle::Drop: return "Drop";
	case TextAppearStyle::Spin: return "Spin";
	case TextAppearStyle::Pop:  return "Pop";
	default:                    return "Fade";
	}
}

TextAppearStyle TextAppearStyleFromString(const std::string& value)
{
	if (value == "Drop") { return TextAppearStyle::Drop; }
	if (value == "Spin") { return TextAppearStyle::Spin; }
	if (value == "Pop") { return TextAppearStyle::Pop; }
	return TextAppearStyle::Fade;
}

namespace
{
constexpr wchar_t kNewLine = L'\n';
constexpr wchar_t kMissingGlyph = L'?';
constexpr float kHalf = 0.5f;
// Drop で落ちてくる高さ（em 単位）
constexpr float kDropHeight = 1.5f;
// Pop の弾み方（easeOutBack の定数）
constexpr float kBackOvershoot = 1.70158f;

float Clamp01(float value)
{
	return (std::min)((std::max)(value, 0.0f), 1.0f);
}

float EaseOutCubic(float t)
{
	const float inverse = 1.0f - t;
	return 1.0f - inverse * inverse * inverse;
}

float EaseOutBack(float t)
{
	const float c3 = kBackOvershoot + 1.0f;
	const float u = t - 1.0f;
	return 1.0f + c3 * u * u * u + kBackOvershoot * u * u;
}

/**
 * @brief 1文字の出入りの状態
 */
struct CharMotion
{
	Vector3 offset{ 0.0f, 0.0f, 0.0f };
	float spin = 0.0f;
	float scale = 1.0f;
	float alpha = 1.0f;
};

/**
 * @brief 出る途中 appear（0→1）と抜ける途中 leave（0→1）から、動き方ごとの状態を作る
 */
CharMotion MakeMotion(TextAppearStyle style, float appear, float leave, float size)
{
	CharMotion motion;
	const float easedIn = EaseOutCubic(appear);
	const float easedOut = EaseOutCubic(leave);
	switch (style)
	{
	case TextAppearStyle::Drop:
		motion.offset.y = ((1.0f - easedIn) - easedOut) * kDropHeight * size;
		motion.alpha = appear * (1.0f - leave);
		break;
	case TextAppearStyle::Spin:
		motion.spin = ((1.0f - easedIn) + easedOut) * std::numbers::pi_v<float>;
		motion.alpha = appear * (1.0f - leave);
		break;
	case TextAppearStyle::Pop:
		motion.scale = (std::max)(EaseOutBack(appear) * (1.0f - easedOut), 0.0f);
		motion.alpha = Clamp01(appear * 2.0f) * (1.0f - leave);
		break;
	default:
		motion.alpha = easedIn * (1.0f - easedOut);
		break;
	}
	return motion;
}
} // namespace

void TextMesh3D::SetText(const std::string& utf8Text)
{
	// 毎フレーム同じ文字列が来るので、変換（確保）は変わったときだけ
	if (utf8Text == utf8Text_)
	{
		return;
	}
	utf8Text_ = utf8Text;
	text_ = StringUtility::ConvertString(utf8Text_);
	charCount_ = static_cast<size_t>(std::count_if(text_.begin(), text_.end(), [](wchar_t c) { return c != kNewLine; }));
}

void TextMesh3D::BuildInstances(GlyphAtlas& atlas, std::vector<Instance>& out) const
{
	if (text_.empty() || !atlas.IsReady() || atlas.GetFontPixelSize() <= 0.0f)
	{
		return;
	}
	const auto findGlyph = [&](wchar_t c)
	{
		const GlyphInfo* glyph = atlas.Acquire(c);
		return glyph ? glyph : atlas.Acquire(kMissingGlyph);
	};

	// アトラスのピクセル → ワールド。Y はピクセルが下向き、ワールドが上向き
	const float pixelToWorld = params_.size / atlas.GetFontPixelSize();
	const float lineHeight = atlas.GetLineHeight() * pixelToWorld;
	const float ascent = atlas.GetAscent() * pixelToWorld;
	const float inverseTextureSize = 1.0f / static_cast<float>(GlyphAtlas::kTextureSize);

	// 行ごとの幅と行数（中央揃えのため先に測る）
	size_t lineCount = 1;
	for (wchar_t c : text_)
	{
		if (c == kNewLine)
		{
			++lineCount;
		}
	}
	const float blockTop = lineHeight * static_cast<float>(lineCount) * kHalf;

	const Matrix4x4 objectWorld = Multiply(
		Multiply(MakeAffineMatrix({ params_.scale, params_.scale, params_.scale }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }), params_.rotation.Normalized().ToMatrix()),
		MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, params_.position));

	size_t lineStart = 0;
	size_t lineIndex = 0;
	size_t charIndex = 0;
	while (lineStart <= text_.size())
	{
		size_t lineEnd = text_.find(kNewLine, lineStart);
		if (lineEnd == std::wstring::npos)
		{
			lineEnd = text_.size();
		}

		float lineWidth = 0.0f;
		for (size_t i = lineStart; i < lineEnd; ++i)
		{
			if (const GlyphInfo* glyph = findGlyph(text_[i]))
			{
				lineWidth += glyph->advance * pixelToWorld;
			}
		}

		// 行の中心を原点に揃える
		float penX = -lineWidth * kHalf;
		const float baseline = blockTop - lineHeight * static_cast<float>(lineIndex) - ascent;
		for (size_t i = lineStart; i < lineEnd; ++i, ++charIndex)
		{
			const GlyphInfo* glyph = findGlyph(text_[i]);
			if (!glyph)
			{
				continue;
			}
			const float advance = glyph->advance * pixelToWorld;
			const float appear = Clamp01(params_.reveal - static_cast<float>(charIndex));
			const float leave = Clamp01(params_.exit - static_cast<float>(charIndex));
			if (glyph->width > 0.0f && glyph->height > 0.0f && appear > 0.0f && leave < 1.0f)
			{
				const CharMotion motion = MakeMotion(params_.style, appear, leave, params_.size);
				const float width = glyph->width * pixelToWorld;
				const float height = glyph->height * pixelToWorld;
				// 板の中心（ワールドの Y は上向き）
				const Vector3 center = {
					penX + glyph->offsetX * pixelToWorld + width * kHalf + motion.offset.x,
					baseline - glyph->offsetY * pixelToWorld - height * kHalf + motion.offset.y,
					motion.offset.z };
				const Matrix4x4 local = MakeAffineMatrix({ width * motion.scale, height * motion.scale, 1.0f }, { 0.0f, motion.spin, 0.0f }, center);

				Instance instance;
				instance.world = Multiply(local, objectWorld);
				instance.uvRect = {
					glyph->x * inverseTextureSize,
					glyph->y * inverseTextureSize,
					(glyph->x + glyph->width) * inverseTextureSize,
					(glyph->y + glyph->height) * inverseTextureSize };
				instance.color = { params_.color.x, params_.color.y, params_.color.z, params_.color.w * motion.alpha };
				out.push_back(instance);
			}
			penX += advance;
		}

		if (lineEnd == text_.size())
		{
			break;
		}
		lineStart = lineEnd + 1;
		++lineIndex;
	}
}
} // namespace KCE
