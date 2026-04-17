#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include "SpriteCommon.h"
#include "Sprite.h"

/**
 * @brief フォント整列方向
 */
enum class FontAlignment
{
	Left,
	Center,
	Right,
};

/**
 * @brief 文字情報構造体
 * @details フォントアトラスJSON出力と対応
 */
struct CharInfo
{
    int x, y;           // アトラス内の座標
    int w, h;           // セルサイズ
    float u, v;         // UV座標
    float uw, vh;       // UVサイズ
};

/**
 * @brief フォントスプライトクラス
 * @details フォントアトラスPNGとJSONを使用して文字列を描画するクラス
 *
 * SetText / DrawText のタイミングで、その文字列長分のスプライトを生成（または再利用）する実装。
 * これにより文字列中で同一文字が連続していても、それぞれ別インスタンス（別 WVP）で描画されるため
 * 「後ろの文字しか見えない」問題を回避できます。
 */
class FontSprite
{
public:
    FontSprite() = default;
    ~FontSprite() = default;

    /**
     * @brief 初期化
     * @param spriteCommon スプライト共通部へのポインタ
     * @param atlasTexturePath フォントアトラスPNGのパス（fontName を基に内部で ./Resources/fonts/<fontName>_atlas.png を参照）
     */
    void Initialize(SpriteCommon* spriteCommon, const std::string& fontName);

    /**
     * @brief 更新処理
     * @details 設定されている文字列に対応するスプライトを更新
     */
    void Update();

    /**
     * @brief 描画
     * @details SetTextで設定された文字列を描画する
     */
    void Draw();

    /**
     * @brief 文字列描画（即座に描画）
     * @param text 描画する文字列
     * @param position 描画開始位置
     * @param scale スケール（デフォルト1.0f）
     * @param spacing 文字間スペース（デフォルト0.0f）
     *
     * DrawText 呼び出し時にも EnsureSpritesForText を実行して必要な分だけスプライトを用意します。
     */
    void DrawText(const std::string& text, const Vector2& position, float scale = 1.0f, float spacing = 0.0f);

    /**
     * @brief 単一文字描画
     * @param character 描画する文字
     * @param position 描画位置
     * @param scale スケール
     */
    void DrawChar(char character, const Vector2& position, float scale = 1.0f);

    /*---------------[ セッター ]---------------*/

    /**
     * @brief 表示する文字列を設定
     * @param text 表示する文字列
     * @note SetText のタイミングでスプライトが Ensure されます
     */
    void SetText(const std::string& text);

    /**
     * @brief 描画位置を設定
     * @param position 描画開始位置
     */
    void SetPosition(const Vector2& position) { position_ = position; }

    /**
     * @brief スケールを設定
     * @param scale スケール値
     */
    void SetScale(float scale) { scale_ = scale; }

    /**
     * @brief 文字間スペースを設定
     * @param spacing 文字間スペース
     */
    void SetSpacing(float spacing) { spacing_ = spacing; }

    /**
     * @brief 行間スペースを設定
     * @param lineSpacing 行間スペース
     */
    void SetLineSpacing(float lineSpacing) { lineSpacing_ = lineSpacing; }

    /**
     * @brief 色を設定（全文字共通）
     * @param color 色（RGBA）
     */
    void SetColor(const Vector4& color) { color_ = color; }

    /**
     * @brief 回転を設定
     * @param rotation 回転角度（ラジアン）
     */
    void SetRotation(float rotation) { rotation_ = rotation; }

    /**
     * @brief 表示/非表示を設定
     * @param isVisible 表示するならtrue
     */
    void SetVisible(bool isVisible) { isVisible_ = isVisible; }

	/**
	 * @brief 整列方向を設定
	 * @param alignment 整列方向
	 */
	void SetAlignment(FontAlignment alignment) { alignment_ = alignment; }

    /*---------------[ ゲッター ]---------------*/

    /**
     * @brief 表示する文字列を取得
     * @return 現在の文字列
     */
    const std::string& GetText() const { return text_; }

    /**
     * @brief 描画位置を取得
     * @return 現在の描画位置
     */
    const Vector2& GetPosition() const { return position_; }

    /**
     * @brief スケールを取得
     * @return 現在のスケール
     */
    float GetScale() const { return scale_; }

    /**
     * @brief 文字間スペースを取得
     * @return 現在の文字間スペース
     */
    float GetSpacing() const { return spacing_; }

    /**
     * @brief 行間スペースを取得
     * @return 現在の行間スペース
     */
    float GetLineSpacing() const { return lineSpacing_; }

    /**
     * @brief 色を取得
     * @return 現在の色
     */
    const Vector4& GetColor() const { return color_; }

    /**
     * @brief 表示状態を取得
     * @return 表示中ならtrue
     */
    bool IsVisible() const { return isVisible_; }

    /**
     * @brief セルサイズを取得
     * @return セルサイズ
     */
    float GetCellSize() const { return cellSize_; }

	/**
	 * @brief 文字列表示時の総幅を計算して返す
	 * @return 総幅（ピクセル）
	 */
	float GetTextWidth() const;

private:
    /**
     * @brief JSONファイルからフォントメトリクスを読み込む
     * @param jsonPath JSONファイルパス
     */
    void LoadFontMetrics(const std::string& jsonPath);

    /**
     * @brief SetText / DrawText のタイミングで必要分のスプライトを確保する
     * @param text 対象の文字列
     * @details 既存スプライトは再利用し、足りない分だけ生成します。スペースは nullptr として扱います。
     */
    void EnsureSpritesForText(const std::string& text);

    /**
     * @brief 指定した文字のスプライトを新規生成して返す
     * @param c 文字
     * @return 作成したスプライト（失敗時は nullptr）
     */
    std::unique_ptr<Sprite> CreateSpriteForChar(char c);

    /**
     * @brief 内部実装：文字列を描画
     */
    void DrawTextInternal(const std::string& text, const Vector2& position, float scale, float spacing);

private:
    // スプライト共通部
    SpriteCommon* spriteCommon_ = nullptr;

    // フォントアトラステクスチャパス
    std::string atlasTexturePath_;

    // 文字情報マップ（JSONから読み込み）
    std::unordered_map<char, CharInfo> charMetrics_;

    // セルサイズ（フォントアトラス生成時の値）
    float cellSize_ = 64.0f;

    // indexごとのスプライト配列（text の各文字位置に対応）
    std::vector<std::unique_ptr<Sprite>> indexSprites_;

    // 表示する文字列
    std::string text_;

    // 描画位置
    Vector2 position_ = { 0.0f, 0.0f };

    // スケール
    float scale_ = 1.0f;

    // 文字間スペース
    float spacing_ = 0.0f;

    // 行間スペース
    float lineSpacing_ = 0.0f;

    // 色
    Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };

    // 回転
    float rotation_ = 0.0f;

    // 表示フラグ
    bool isVisible_ = true;

	// 整列方向
	FontAlignment alignment_ = FontAlignment::Left;
};