#include "NumberSprite.h"
#include <vector>
#include <algorithm>

// 10進数の基数
constexpr int kDecimalBase = 10;
// 数字テクスチャのY座標開始位置
constexpr float kTextureTopY = 0.0f;
// アンカーポイントの中心位置
constexpr float kAnchorCenter = 0.5f;

void NumberSprite::Initialize(SpriteCommon* spriteCommon, const std::string& textureFilePath, const KCE::Vector2& digit)
{
	digit_ = digit;

	// すべての桁分のSpriteを生成・初期化
	for (auto& sprite : digits_)
	{
		sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon, textureFilePath);

		// 各桁は初期状態で全体表示（左上座標とサイズは描画時に設定）
		sprite->SetTextureLeftTop(KCE::Vector2(0.0f, 0.0f));
		sprite->SetTextureSize(digit_);
		sprite->SetSize(digit_);

		// 中心を基準にアンカーポイントを設定
		sprite->SetAnchorPoint(KCE::Vector2(kAnchorCenter, kAnchorCenter));
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

void NumberSprite::Draw()
{
	// 非表示なら描画しない
	if (!isVisible_) return;

	// 設定された数値を描画
	DrawNumber(number_, position_, spacing_);
}

void NumberSprite::DrawDigit(int number, const KCE::Vector2& position)
{
	// 非表示なら描画しない
	if (!isVisible_) return;

	if (digits_.empty()) return;

	// 0-9の範囲外は描画しない
	if (number < 0 || number > 9) return;

	// 先頭のSpriteだけ使う（複数桁表示の場合はDrawNumberを使う）
	auto& sprite = digits_[0];

	// テクスチャ内の対応する数字位置を計算
	float srcX = digit_.x * number;
	sprite->SetTextureLeftTop(KCE::Vector2(srcX, kTextureTopY));
	sprite->SetPosition(position);
	sprite->Draw();
}

void NumberSprite::DrawNumber(int number, const KCE::Vector2& position, float spacing)
{
	// 非表示なら描画しない
	if (!isVisible_) return;

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

	// 実際の桁数
	int actualDigitCount = static_cast<int>(digitValues.size());

	// 表示する桁数（最小桁数を考慮）
	int displayDigitCount = (minDigits_ > actualDigitCount) ? minDigits_ : actualDigitCount;

	// スケールを適用したサイズ
	KCE::Vector2 scaledSize = { digit_.x * scale_, digit_.y * scale_ };

	// 揃え位置に応じた開始位置を計算
	KCE::Vector2 startPos = CalculateStartPosition(displayDigitCount);

	// ゼロ埋めが必要な桁数
	int leadingZeros = displayDigitCount - actualDigitCount;

	// 左詰で描画
	KCE::Vector2 pos = startPos;
	int spriteIndex = 0;

	// 先頭のゼロを描画（ゼロ埋め）
	for (int i = 0; i < leadingZeros && spriteIndex < static_cast<int>(digits_.size()); ++i)
	{
		float srcX = 0.0f; // 0のテクスチャ位置
		digits_[spriteIndex]->SetTextureLeftTop(KCE::Vector2(srcX, kTextureTopY));
		digits_[spriteIndex]->SetPosition(pos);
		digits_[spriteIndex]->SetSize(scaledSize);
		digits_[spriteIndex]->SetColor(color_);
		digits_[spriteIndex]->Draw();

		pos.x += scaledSize.x + spacing;
		spriteIndex++;
	}

	// 実際の数字を描画
	for (size_t i = 0; i < digitValues.size() && spriteIndex < static_cast<int>(digits_.size()); ++i)
	{
		int d = digitValues[i];

		// テクスチャ内の対応する数字位置を計算
		float srcX = digit_.x * d;
		digits_[spriteIndex]->SetTextureLeftTop(KCE::Vector2(srcX, kTextureTopY));
		digits_[spriteIndex]->SetPosition(pos);
		digits_[spriteIndex]->SetSize(scaledSize);
		digits_[spriteIndex]->SetColor(color_);
		digits_[spriteIndex]->Draw();

		// 次の桁の位置を計算（スケール適用）
		pos.x += scaledSize.x + spacing;
		spriteIndex++;
	}
}

int NumberSprite::GetDigitCount(int number) const
{
	if (number == 0) return 1;

	int count = 0;
	int temp = number;
	while (temp > 0)
	{
		count++;
		temp /= kDecimalBase;
	}
	return count;
}

KCE::Vector2 NumberSprite::CalculateStartPosition(int digitCount) const
{
	// スケールを適用したサイズ
	float scaledWidth = digit_.x * scale_;

	// 全体の幅を計算
	float totalWidth = scaledWidth * digitCount + spacing_ * (digitCount - 1);

	KCE::Vector2 startPos = position_;

	switch (alignment_)
	{
	case NumberAlignment::Left:
		// 左揃え：position_がそのまま左端
		break;

	case NumberAlignment::Center:
		// 中央揃え：position_を中心として左右に配置
		startPos.x = position_.x - totalWidth * 0.5f + scaledWidth * 0.5f;
		break;

	case NumberAlignment::Right:
		// 右揃え：position_が右端になるように左にオフセット
		startPos.x = position_.x - totalWidth + scaledWidth * 0.5f;
		break;
	}

	return startPos;
}

void NumberSprite::SetDigitSize(const KCE::Vector2& size)
{
	digit_ = size;

	// 全てのスプライトにサイズを反映
	for (auto& sprite : digits_)
	{
		if (sprite)
		{
			sprite->SetTextureSize(size);
			sprite->SetSize(size);
		}
	}
}

void NumberSprite::SetColor(const Vector4& color)
{
	color_ = color;

	// 全てのスプライトに色を反映
	for (auto& sprite : digits_)
	{
		if (sprite)
		{
			sprite->SetColor(color);
		}
	}
}

void NumberSprite::SetScale(float scale)
{
	scale_ = scale;
}
