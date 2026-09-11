#include "graphics/2d/TextSprite.h"

#include "base/StringUtility.h"
#include "graphics/2d/GlyphAtlas.h"
#include "graphics/2d/Sprite.h"

namespace KCE
{
namespace
{
// アトラスに無い文字の代わり
constexpr wchar_t kMissingGlyph = L'?';
constexpr wchar_t kNewLine = L'\n';
constexpr wchar_t kSpace = L' ';
constexpr wchar_t kIdeographicSpace = L'　';
constexpr float kHalf = 0.5f;

bool SameVector(const Vector2& a, const Vector2& b) { return a.x == b.x && a.y == b.y; }
bool SameVector(const Vector4& a, const Vector4& b) { return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; }
} // namespace

TextSprite::TextSprite() = default;
TextSprite::~TextSprite() = default;

void TextSprite::Initialize(SpriteCommon* spriteCommon, const GlyphAtlas* atlas)
{
	spriteCommon_ = spriteCommon;
	atlas_ = atlas;
	dirty_ = true;
}

void TextSprite::SetText(const std::string& utf8Text)
{
	// 毎フレーム同じ文字列が来るので、変換（確保）は変わったときだけ
	if (utf8Text == utf8Text_)
	{
		return;
	}
	utf8Text_ = utf8Text;
	text_ = StringUtility::ConvertString(utf8Text_);
	dirty_ = true;
}

void TextSprite::SetPosition(const Vector2& position)
{
	if (!SameVector(position, position_)) { position_ = position; dirty_ = true; }
}

void TextSprite::SetFontSize(float pixelSize)
{
	if (pixelSize != fontSize_) { fontSize_ = pixelSize; dirty_ = true; }
}

void TextSprite::SetColor(const Vector4& color)
{
	if (!SameVector(color, color_)) { color_ = color; dirty_ = true; }
}

void TextSprite::SetAlign(TextAlign align)
{
	if (align != align_) { align_ = align; dirty_ = true; }
}

void TextSprite::SetMaxWidth(float width)
{
	if (width != maxWidth_) { maxWidth_ = width; dirty_ = true; }
}

void TextSprite::SetVisibleCount(size_t count)
{
	if (count != visibleCount_) { visibleCount_ = count; dirty_ = true; }
}

void TextSprite::Draw()
{
	if (dirty_)
	{
		Layout();
	}
	for (size_t i = 0; i < activeCount_; ++i)
	{
		sprites_[i]->Draw();
	}
}

Sprite* TextSprite::AcquireSprite()
{
	if (activeCount_ == sprites_.size())
	{
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon_, GlyphAtlas::kTextureKey);
		sprites_.push_back(std::move(sprite));
	}
	return sprites_[activeCount_++].get();
}

void TextSprite::Layout()
{
	dirty_ = false;
	activeCount_ = 0;
	if (!spriteCommon_ || !atlas_ || !atlas_->IsReady() || atlas_->GetFontPixelSize() <= 0.0f)
	{
		return;
	}

	const float scale = fontSize_ / atlas_->GetFontPixelSize();
	const float lineHeight = atlas_->GetLineHeight() * scale;
	const GlyphInfo* missing = atlas_->Find(kMissingGlyph);
	const auto findGlyph = [&](wchar_t c)
	{
		const GlyphInfo* glyph = atlas_->Find(c);
		return glyph ? glyph : missing;
	};

	// 行に分ける。日本語は単語の区切りが無いので、幅を超えたら文字単位で折り返す
	lines_.clear();
	Line line;
	for (size_t i = 0; i < text_.size(); ++i)
	{
		const wchar_t c = text_[i];
		if (c == kNewLine)
		{
			line.end = i;
			lines_.push_back(line);
			line = { i + 1, i + 1, 0.0f };
			continue;
		}
		const GlyphInfo* glyph = findGlyph(c);
		const float advance = glyph ? glyph->advance * scale : 0.0f;
		if (maxWidth_ > 0.0f && i > line.begin && line.width + advance > maxWidth_)
		{
			line.end = i;
			lines_.push_back(line);
			line = { i, i, 0.0f };
		}
		line.width += advance;
	}
	line.end = text_.size();
	lines_.push_back(line);

	size_t shown = 0;
	for (size_t lineIndex = 0; lineIndex < lines_.size(); ++lineIndex)
	{
		const Line& current = lines_[lineIndex];
		float x = position_.x;
		if (align_ == TextAlign::Center)
		{
			x -= current.width * kHalf;
		}
		else if (align_ == TextAlign::Right)
		{
			x -= current.width;
		}
		const float y = position_.y + lineHeight * static_cast<float>(lineIndex);

		for (size_t i = current.begin; i < current.end; ++i)
		{
			if (shown >= visibleCount_)
			{
				return;
			}
			++shown;

			const wchar_t c = text_[i];
			const GlyphInfo* glyph = findGlyph(c);
			if (!glyph)
			{
				continue;
			}
			if (c != kSpace && c != kIdeographicSpace)
			{
				Sprite* sprite = AcquireSprite();
				sprite->SetTextureLeftTop({ glyph->x, glyph->y });
				sprite->SetTextureSize({ glyph->width, glyph->height });
				sprite->SetSize({ glyph->width * scale, glyph->height * scale });
				sprite->SetAnchorPoint({ 0.0f, 0.0f });
				sprite->SetPosition({ x, y });
				sprite->SetColor(color_);
				sprite->Update();
			}
			x += glyph->advance * scale;
		}
	}
}
} // namespace KCE
