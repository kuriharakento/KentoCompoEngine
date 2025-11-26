#pragma once
#include "Sprite.h"
#include "SpriteCommon.h"
#include "math/Vector2.h"

/**
 * @brief 数値スプライトクラス
 * @details 数字テクスチャを使用して数値を表示するためのクラス。
 *          intの最大桁数（10桁）まで対応している。
 */
class NumberSprite
{
public:
	NumberSprite() = default;

	/**
	 * @brief 初期化処理
	 * @param spriteCommon スプライト共通部へのポインタ
	 * @param textureFilePath 数字テクスチャのファイルパス
	 * @param digit 1桁のサイズ（幅、高さ）
	 */
	void Initialize(SpriteCommon* spriteCommon, std::string textureFilePath, const Vector2& digit);

	/**
	 * @brief 更新処理
	 * @details すべての桁スプライトを更新する
	 */
	void Update();

	/**
	 * @brief 単一の数字を描画
	 * @param number 描画する数字（0-9）
	 * @param position 描画位置
	 */
	void DrawDigit(int number, const Vector2& position);

	/**
	 * @brief 数値を描画
	 * @param number 描画する数値
	 * @param position 描画開始位置（左端）
	 * @param spacing 桁間のスペース
	 */
	void DrawNumber(int number, const Vector2& position, float spacing);

private:
	// 10桁分のスプライト(intの桁数は最大10桁までなので)
	std::array<std::unique_ptr<Sprite>, 10> digits_;
	// 1桁のサイズ
	Vector2 digit_ = {};
};