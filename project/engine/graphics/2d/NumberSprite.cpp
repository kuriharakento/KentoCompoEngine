#include "NumberSprite.h"
#include <vector>
#include <algorithm>

// 10進数の基数
constexpr int kDecimalBase = 10;
// 数字テクスチャのY座標開始位置
constexpr float kTextureTopY = 0.0f;
// アンカーポイントの中心位置
constexpr float kAnchorCenter = 0.5f;

void NumberSprite::Initialize(SpriteCommon* spriteCommon, std::string textureFilePath, const Vector2& digit)
{
	digit_ = digit;

	// すべての桁分のSpriteを生成・初期化
	for (auto& sprite : digits_)
	{
		sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon, textureFilePath);

		// 各桁は初期状態で全体表示（左上座標とサイズは描画時に設定）
		sprite->SetTextureLeftTop(Vector2(0.0f, 0.0f));
		sprite->SetTextureSize(digit_);
		sprite->SetSize(digit_);

		// 中心を基準にアンカーポイントを設定
		sprite->SetAnchorPoint(Vector2(kAnchorCenter, kAnchorCenter));
	}
}

void NumberSprite::Update()
{
	// すべての桁スプライトを更新
	for (auto& sprite : digits_)
	{
		if (sprite) sprite->Update();
	}
}

void NumberSprite::DrawDigit(int number, const Vector2& position)
{
	if (digits_.empty()) return;

	// 0-9の範囲外は描画しない
	if (number < 0 || number > 9) return;

	// 先頭のSpriteだけ使う（複数桁表示の場合はDrawNumberを使う）
	auto& sprite = digits_[0];

	// テクスチャ内の対応する数字位置を計算
	float srcX = digit_.x * number;
	sprite->SetTextureLeftTop(Vector2(srcX, kTextureTopY));
	sprite->SetPosition(position);
	sprite->Draw();
}

void NumberSprite::DrawNumber(int number, const Vector2& position, float spacing)
{
	// 桁ごとに分解
	std::vector<int> digitValues;
	int temp = number;

	// 0の場合は0を追加
	if (temp == 0) digitValues.push_back(0);

	// 各桁の数値を抽出（下位桁から）
	while (temp > 0 && digitValues.size() < digits_.size())
	{
		digitValues.push_back(temp % kDecimalBase);
		temp /= kDecimalBase;
	}

	// 上位桁から描画するため反転
	std::reverse(digitValues.begin(), digitValues.end());

	// 左詰で描画
	Vector2 pos = position;
	for (size_t i = 0; i < digitValues.size(); ++i)
	{
		int d = digitValues[i];

		// テクスチャ内の対応する数字位置を計算
		float srcX = digit_.x * d;
		digits_[i]->SetTextureLeftTop(Vector2(srcX, kTextureTopY));
		digits_[i]->SetPosition(pos);
		digits_[i]->Draw();

		// 次の桁の位置を計算
		pos.x += digit_.x + spacing;
	}
}