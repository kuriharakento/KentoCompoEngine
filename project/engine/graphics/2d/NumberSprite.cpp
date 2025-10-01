#include "NumberSprite.h"
#include <vector>
#include <algorithm>

void NumberSprite::Initialize(SpriteCommon* spriteCommon, std::string textureFilePath, const Vector2& digit)
{
	digit_ = digit;
	// 4桁分のSpriteを生成・初期化
	for (auto& sprite : digits_)
	{
		sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon, textureFilePath);
		// 各桁は初期状態で全体表示（左上座標とサイズは描画時に設定）
		sprite->SetTextureLeftTop(Vector2(0.0f, 0.0f));
		sprite->SetTextureSize(digit_);
		sprite->SetSize(digit_);
		sprite->SetAnchorPoint(Vector2(0.5f, 0.5f)); // 中心を基準に
	}
}

void NumberSprite::Update()
{
	for (auto& sprite : digits_)
	{
		if (sprite) sprite->Update();
	}
}

void NumberSprite::DrawDigit(int number, const Vector2& position)
{
	if (digits_.empty()) return;
	if (number < 0 || number > 9) return;
	// 先頭のSpriteだけ使う（複数桁表示の場合はDrawNumberを使う）
	auto& sprite = digits_[0];
	float srcX = digit_.x * number;
	float srcY = 0.0f;
	sprite->SetTextureLeftTop(Vector2(srcX, srcY));
	sprite->SetPosition(position);
	sprite->Draw();
}

void NumberSprite::DrawNumber(int number, const Vector2& position, float spacing)
{
	// 桁ごとに分解（最大10桁）
	std::vector<int> digitValues;
	int temp = number;
	if (temp == 0) digitValues.push_back(0);
	while (temp > 0 && digitValues.size() < digits_.size())
	{
		digitValues.push_back(temp % 10);
		temp /= 10;
	}
	std::reverse(digitValues.begin(), digitValues.end());

	// 左詰で描画（桁数が10未満の場合は左寄せ）
	Vector2 pos = position;
	for (size_t i = 0; i < digitValues.size(); ++i)
	{
		int d = digitValues[i];
		float srcX = digit_.x * d;
		float srcY = 0.0f;
		digits_[i]->SetTextureLeftTop(Vector2(srcX, srcY));
		digits_[i]->SetPosition(pos);
		digits_[i]->Draw();
		pos.x += digit_.x + spacing; // 次の桁へ
	}
}