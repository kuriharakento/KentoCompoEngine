#pragma once
#include "Sprite.h"
#include "SpriteCommon.h"
#include "math/Vector2.h"

class NumberSprite
{
public:
	NumberSprite() = default;

	void Initialize(SpriteCommon* spriteCommon, std::string textureFilePath, const Vector2& digit);
	void Update();

	// 単一の数字を描画
	void DrawDigit(int number, const Vector2& position);

	// 数値を描画
	void DrawNumber(int number, const Vector2& position);

private:
	// 10桁分のスプライト(intの桁数は最大10桁までなので)
	std::array<std::unique_ptr<Sprite>, 10> digits_;
	Vector2 digit_ = {}; // 1桁のサイズ
};