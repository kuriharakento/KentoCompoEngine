#pragma once
#include <memory>
#include <string>
#include "graphics/2d/FontSprite.h"
#include "graphics/2d/SpriteCommon.h"

/**
 * @class ControlsGuide
 * @brief 操作説明UIを表示するクラス
 *
 * 複数シーンで再利用可能な操作ガイドUI。
 * FontSpriteを使用してテキストベースで操作方法を表示する。
 */
class ControlsGuide
{
public:
	/**
	 * @brief 初期化処理
	 * @param spriteCommon スプライト共通設定
	 * @param fontName フォント名（Resources/fonts/配下の名前）
	 */
	void Initialize(SpriteCommon* spriteCommon, const std::string& fontName);

	/**
	 * @brief 更新処理
	 */
	void Update();

	/**
	 * @brief 描画処理
	 */
	void Draw();

	/**
	 * @brief 表示テキストを設定（デフォルトを上書き）
	 * @param text 表示するテキスト（改行は\nで指定）
	 */
	void SetText(const std::string& text);

	/**
	 * @brief 表示位置を設定
	 * @param position 表示位置
	 */
	void SetPosition(const Vector2& position);

	/**
	 * @brief 文字サイズを設定
	 * @param scale スケール値（1.0がデフォルト）
	 */
	void SetScale(float scale);

	/**
	 * @brief 行間を設定
	 * @param lineSpacing 行間
	 */
	void SetLineSpacing(float lineSpacing);

	/**
	 * @brief 表示/非表示を設定
	 * @param isVisible 表示するかどうか
	 */
	void SetVisible(bool isVisible);

	/**
	 * @brief 表示状態を取得
	 * @return 表示中かどうか
	 */
	bool IsVisible() const;

private:
	// フォントスプライト（テキスト描画用）
	std::unique_ptr<FontSprite> text_;

	// 表示フラグ
	bool isVisible_ = true;
};