#include "ControlsGuide.h"

void ControlsGuide::Initialize(SpriteCommon* spriteCommon, const std::string& fontName)
{
	text_ = std::make_unique<FontSprite>();
	text_->Initialize(spriteCommon, fontName);

	// デフォルトの操作説明テキスト
	text_->SetText(
		"Default Text: default\n"
	);

	// デフォルト表示位置（画面左下付近）
	text_->SetPosition({ 20.0f, 550.0f });
	text_->SetScale(0.5f);
	text_->SetLineSpacing(5.0f);
}

void ControlsGuide::Update()
{
	if (!isVisible_) return;

	text_->Update();
}

void ControlsGuide::Draw()
{
	if (!isVisible_) return;

	text_->Draw();
}

void ControlsGuide::SetText(const std::string& text)
{
	text_->SetText(text);
}

void ControlsGuide::SetPosition(const Vector2& position)
{
	text_->SetPosition(position);
}

void ControlsGuide::SetScale(float scale)
{
	text_->SetScale(scale);
}

void ControlsGuide::SetLineSpacing(float lineSpacing)
{
	text_->SetLineSpacing(lineSpacing);
}

void ControlsGuide::SetVisible(bool isVisible)
{
	isVisible_ = isVisible;
	text_->SetVisible(isVisible);
}

bool ControlsGuide::IsVisible() const
{
	return isVisible_;
}
