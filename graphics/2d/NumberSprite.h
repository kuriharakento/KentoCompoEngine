#pragma once
#include "Sprite.h"
#include "SpriteCommon.h"

/**
 * @brief 数値の表示揃え位置
 */
enum class NumberAlignment
{
	Left,   // 左揃え
	Center, // 中央揃え
	Right   // 右揃え
};

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
	void Initialize(SpriteCommon* spriteCommon, const std::string& textureFilePath, const KCE::Vector2& digit);

	/**
	 * @brief 更新処理
	 * @details すべての桁スプライトを更新する
	 */
	void Update();

	/**
	 * @brief 描画処理
	 * @details SetNumberで設定された数値を描画する
	 */
	void Draw();

	/**
	 * @brief 単一の数字を描画
	 * @param number 描画する数字（0-9）
	 * @param position 描画位置
	 */
	void DrawDigit(int number, const KCE::Vector2& position);

	/**
	 * @brief 数値を描画
	 * @param number 描画する数値
	 * @param position 描画開始位置（左端）
	 * @param spacing 桁間のスペース
	 */
	void DrawNumber(int number, const KCE::Vector2& position, float spacing);

	/*---------------[ ゲッター ]---------------*/

	/**
	 * @brief 表示する数値を取得
	 * @return 現在の表示数値
	 */
	int GetNumber() const { return number_; }

	/**
	 * @brief 描画位置を取得
	 * @return 現在の描画位置
	 */
	const KCE::Vector2& GetPosition() const { return position_; }

	/**
	 * @brief 桁間のスペースを取得
	 * @return 現在の桁間スペース
	 */
	float GetSpacing() const { return spacing_; }

	/**
	 * @brief 1桁のサイズを取得
	 * @return 1桁のサイズ（幅、高さ）
	 */
	const KCE::Vector2& GetDigitSize() const { return digit_; }

	/**
	 * @brief 色を取得
	 * @return 現在の色（RGBA）
	 */
	const Vector4& GetColor() const { return color_; }

	/**
	 * @brief 表示状態を取得
	 * @return 表示中ならtrue
	 */
	bool IsVisible() const { return isVisible_; }

	/**
	 * @brief 揃え位置を取得
	 * @return 現在の揃え位置
	 */
	NumberAlignment GetAlignment() const { return alignment_; }

	/**
	 * @brief 最小表示桁数を取得
	 * @return 最小表示桁数
	 */
	int GetMinDigits() const { return minDigits_; }

	/*---------------[ セッター ]---------------*/

	/**
	 * @brief 表示する数値を設定
	 * @param number 表示する数値
	 */
	void SetNumber(int number) { number_ = number; }

	/**
	 * @brief 描画位置を設定
	 * @param position 描画位置
	 */
	void SetPosition(const KCE::Vector2& position) { position_ = position; }

	/**
	 * @brief 桁間のスペースを設定
	 * @param spacing 桁間のスペース
	 */
	void SetSpacing(float spacing) { spacing_ = spacing; }

	/**
	 * @brief 1桁のサイズを設定
	 * @param size 1桁のサイズ（幅、高さ）
	 */
	void SetDigitSize(const KCE::Vector2& size);

	/**
	 * @brief 色を設定
	 * @param color 色（RGBA）
	 */
	void SetColor(const Vector4& color);

	/**
	 * @brief 表示/非表示を設定
	 * @param isVisible 表示するならtrue
	 */
	void SetVisible(bool isVisible) { isVisible_ = isVisible; }

	/**
	 * @brief スケールを設定
	 * @param scale スケール値
	 */
	void SetScale(float scale);

	/**
	 * @brief 揃え位置を設定
	 * @param alignment 揃え位置（Left, Center, Right）
	 */
	void SetAlignment(NumberAlignment alignment) { alignment_ = alignment; }

	/**
	 * @brief 最小表示桁数を設定（ゼロ埋めまたは固定幅表示用）
	 * @param minDigits 最小桁数（0で無効）
	 */
	void SetMinDigits(int minDigits) { minDigits_ = minDigits; }

private:
	/**
	 * @brief 数値の桁数を取得
	 * @param number 数値
	 * @return 桁数
	 */
	int GetDigitCount(int number) const;

	/**
	 * @brief 揃え位置に応じた描画開始位置を計算
	 * @param digitCount 実際の桁数
	 * @return 描画開始位置
	 */
	KCE::Vector2 CalculateStartPosition(int digitCount) const;

private:
	// 10桁分のスプライト(intの桁数は最大10桁までなので)
	std::array<std::unique_ptr<Sprite>, 10> digits_;
	// 1桁のサイズ
	KCE::Vector2 digit_ = {};
	// 表示する数値
	int number_ = 0;
	// 描画位置
	KCE::Vector2 position_ = {};
	// 桁間のスペース
	float spacing_ = 0.0f;
	// 色
	Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
	// 表示フラグ
	bool isVisible_ = true;
	// スケール
	float scale_ = 1.0f;
	// 揃え位置
	NumberAlignment alignment_ = NumberAlignment::Right;
	// 最小表示桁数（0で無効）
	int minDigits_ = 0;
};
