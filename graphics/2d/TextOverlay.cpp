#include "graphics/2d/TextOverlay.h"

#include "graphics/2d/Sprite.h"
#include "graphics/2d/SpriteCommon.h"
#include "manager/graphics/TextureManager.h"

namespace KCE
{
namespace
{
// 座標は 1920x1080 の仮想画面
constexpr Vector2 kLyricPosition = { 960.0f, 930.0f };
// 会話枠が出ている間は、枠と重ならないようにこの位置へ上げる
constexpr Vector2 kLyricAboveBoxPosition = { 960.0f, 660.0f };
constexpr float kLyricFontSize = 56.0f;
constexpr Vector4 kLyricColor = { 1.0f, 1.0f, 1.0f, 1.0f };
constexpr Vector2 kLyricShadowOffset = { 3.0f, 3.0f };
constexpr Vector4 kLyricShadowColor = { 0.0f, 0.0f, 0.0f, 0.7f };

// 会話枠は白1x1のテクスチャを伸ばして半透明の黒にする
constexpr const char* kBoxTexture = "textures/white1x1.png";
constexpr Vector2 kBoxPosition = { 160.0f, 740.0f };
constexpr Vector2 kBoxSize = { 1600.0f, 280.0f };
constexpr Vector4 kBoxColor = { 0.0f, 0.0f, 0.0f, 0.6f };
constexpr Vector2 kSpeakerPosition = { 200.0f, 760.0f };
constexpr float kSpeakerFontSize = 40.0f;
constexpr Vector4 kSpeakerColor = { 1.0f, 0.85f, 0.45f, 1.0f };
constexpr Vector2 kBodyPosition = { 200.0f, 824.0f };
constexpr float kBodyFontSize = 44.0f;
constexpr float kBodyMaxWidth = 1520.0f;
constexpr Vector4 kBodyColor = { 1.0f, 1.0f, 1.0f, 1.0f };

Vector4 WithAlpha(const Vector4& color, float alpha)
{
	return { color.x, color.y, color.z, color.w * alpha };
}
} // namespace

TextOverlay::TextOverlay() = default;
TextOverlay::~TextOverlay() = default;

void TextOverlay::Initialize(SpriteCommon* spriteCommon, GlyphAtlas* atlas, TextSpritePipeline* textPipeline)
{
	spriteCommon_ = spriteCommon;
	lyric_.Initialize(textPipeline, atlas);
	lyric_.SetPosition(kLyricPosition);
	lyric_.SetFontSize(kLyricFontSize);
	lyric_.SetAlign(TextAlign::Center);

	lyricShadow_.Initialize(textPipeline, atlas);
	lyricShadow_.SetPosition({ kLyricPosition.x + kLyricShadowOffset.x, kLyricPosition.y + kLyricShadowOffset.y });
	lyricShadow_.SetFontSize(kLyricFontSize);
	lyricShadow_.SetAlign(TextAlign::Center);

	speaker_.Initialize(textPipeline, atlas);
	speaker_.SetPosition(kSpeakerPosition);
	speaker_.SetFontSize(kSpeakerFontSize);

	body_.Initialize(textPipeline, atlas);
	body_.SetPosition(kBodyPosition);
	body_.SetFontSize(kBodyFontSize);
	body_.SetMaxWidth(kBodyMaxWidth);

	TextureManager::GetInstance()->LoadTexture(kBoxTexture);
	dialogueBox_ = std::make_unique<Sprite>();
	dialogueBox_->Initialize(spriteCommon, kBoxTexture);
	dialogueBox_->SetPosition(kBoxPosition);
	dialogueBox_->SetSize(kBoxSize);
	dialogueBox_->SetColor(kBoxColor);
	dialogueBox_->Update();
}

void TextOverlay::ShowLyric(const std::string& text, float alpha)
{
	lyricVisible_ = alpha > 0.0f && !text.empty();
	if (!lyricVisible_)
	{
		return;
	}
	lyric_.SetText(text);
	lyric_.SetColor(WithAlpha(kLyricColor, alpha));
	lyricShadow_.SetText(text);
	lyricShadow_.SetColor(WithAlpha(kLyricShadowColor, alpha));
}

void TextOverlay::HideLyric()
{
	lyricVisible_ = false;
}

void TextOverlay::ShowDialogue(const std::string& speaker, const std::string& text, size_t visibleCount, float alpha)
{
	dialogueVisible_ = alpha > 0.0f;
	if (!dialogueVisible_)
	{
		return;
	}
	speaker_.SetText(speaker);
	speaker_.SetColor(WithAlpha(kSpeakerColor, alpha));
	body_.SetText(text);
	body_.SetVisibleCount(visibleCount);
	body_.SetColor(WithAlpha(kBodyColor, alpha));
	if (dialogueBox_)
	{
		dialogueBox_->SetColor(WithAlpha(kBoxColor, alpha));
	}
}

void TextOverlay::HideDialogue()
{
	dialogueVisible_ = false;
}

void TextOverlay::Draw()
{
	if (lyricVisible_)
	{
		const Vector2 base = dialogueVisible_ ? kLyricAboveBoxPosition : kLyricPosition;
		lyric_.SetPosition(base);
		lyricShadow_.SetPosition({ base.x + kLyricShadowOffset.x, base.y + kLyricShadowOffset.y });
		lyricShadow_.Draw();
		lyric_.Draw();
	}
	if (dialogueVisible_)
	{
		if (dialogueBox_)
		{
			// 先に描いた歌詞で文字用の設定になっているので、Sprite 用に戻してから描く
			if (spriteCommon_)
			{
				spriteCommon_->CommonRenderingSetting();
			}
			dialogueBox_->Draw();
		}
		speaker_.Draw();
		body_.Draw();
	}
}
} // namespace KCE
