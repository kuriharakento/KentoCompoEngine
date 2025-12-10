#pragma once
#include <memory>
#include "graphics/2d/Sprite.h"
#include "math/Easing.h"

/**
 * @brief レターボックスアニメーションのイージングタイチE
 * 
 * シネ�EチE��チE��な演�Eで使用するレターボックスの表示/非表示アニメーションに適用するイージング関数を定義します、E
 * 吁E��ージングタイプ�E異なる動き�E特性を持ち、演�Eに応じて使ぁE�Eけることができます、E
 */
enum class LetterboxEase
{
    Linear,         ///< 線形補間�E�等速！E
    InSine,         ///< サイン曲線による加送E
    OutSine,        ///< サイン曲線による減送E
    InOutSine,      ///< サイン曲線による加速�E減送E
    InQuint,        ///< 5乗による加送E
    OutQuint,       ///< 5乗による減送E
    InOutQuint,     ///< 5乗による加速�E減送E
    InCirc,         ///< 冁E��曲線による加送E
    OutCirc,        ///< 冁E��曲線による減送E
    InOutCirc,      ///< 冁E��曲線による加速�E減送E
    InElastic,      ///< 弾性皁E��動き�E�開始時にオーバ�Eシュート！E
    OutElastic,     ///< 弾性皁E��動き�E�終亁E��にオーバ�Eシュート！E
    InOutElastic,   ///< 弾性皁E��動き�E�両端でオーバ�Eシュート！E
    InExpo,         ///< 持E��関数による加送E
    OutExpo,        ///< 持E��関数による減送E
    InOutExpo,      ///< 持E��関数による加速�E減送E
    OutQuad,        ///< 2乗による減送E
    InOutQuart,     ///< 4乗による加速�E減送E
    InBack,         ///< 後ろに引いてから加送E
    OutBack,        ///< 前に出てから減送E
    InOutBack,      ///< 後ろに引いて前に出る加速�E減送E
    OutBounce,      ///< バウンドする減送E
    InBounce,       ///< バウンドする加送E
    InOutBounce     ///< バウンドする加速�E減送E
};

/**
 * @brief レターボックスの表示状慁E
 * 
 * レターボックスエフェクト�E現在の状態を管琁E��ます、E
 * 状態�E移により、E��刁E��アニメーション処琁E��実行されます、E
 */
enum class LetterboxState
{
    Hidden,    ///< 完�Eに隠れてぁE��状態（レターボックスが非表示�E�E
    Showing,   ///< 表示アニメーション中
    Visible,   ///< 完�Eに表示されてぁE��状態（レターボックスが画面に表示�E�E
    Hiding     ///< 非表示アニメーション中
};

/**
 * @brief シネ�EチE��チE��レターボックスエフェクトクラス
 * 
 * 映画のような演�Eを実現するための、画面上下に表示される黒帯�E�レターボックス�E�エフェクトを管琁E��ます、E
 * イージング機�Eにより、スムーズなアニメーションで表示・非表示を制御できます、E
 * 
 * 主な機�E:
 * - 多様なイージングタイプによる柔軟なアニメーション
 * - オーバ�Eシュートに対応した描画篁E��管琁E
 * - カスタマイズ可能な色と高さ設宁E
 * - ImGuiによるリアルタイムパラメータ調整
 * 
 * @code
 * // 使用侁E
 * CinematicLetterbox letterbox;
 * letterbox.Initialize(spriteCommon, texturePath, 1280.0f, 720.0f);
 * letterbox.SetEaseType(LetterboxEase::InOutBack);
 * letterbox.Show(1.0f);  // 1秒かけて表示
 * @endcode
 */
class CinematicLetterbox
{
public:
    CinematicLetterbox();
    ~CinematicLetterbox();

    /**
     * @brief レターボックスの初期匁E
     * 
     * スプライトを作�Eし、画面サイズに応じた�E期設定を行います、E
     * 上下に配置されめEつのバ�Eは、オーバ�Eシュート用のマ�Eジンを含めて作�Eされます、E
     * 
     * @param spriteCommon スプライト�E通設定へのポインタ
     * @param texturePath レターボックスに使用するチE��スチャのパス
     * @param screenWidth 画面幁E��ピクセル�E�E
     * @param screenHeight 画面高さ�E�ピクセル�E�E
     */
    void Initialize(SpriteCommon* spriteCommon, const std::string& texturePath, float screenWidth, float screenHeight);

    /**
     * @brief レターボックスを表示
     * 
     * 非表示状態また�E非表示中の場合にのみ、表示アニメーションを開始します、E
     * 既に表示中また�E表示済みの場合�E何もしません、E
     * 
     * @param duration アニメーション時間�E�秒）、デフォルト�E1.0私E
     */
    void Show(float duration = 1.0f);

    /**
     * @brief レターボックスを非表示
     * 
     * 表示状態また�E表示中の場合にのみ、E��表示アニメーションを開始します、E
     * 既に非表示中また�E非表示済みの場合�E何もしません、E
     * 
     * @param duration アニメーション時間�E�秒）、デフォルト�E1.0私E
     */
    void Hide(float duration = 1.0f);

    /**
     * @brief レターボックスの更新処琁E
     * 
     * 毎フレーム呼び出され、アニメーション進行状態を更新します、E
     * 状態に応じてイージング計算を行い、バーの位置を更新します、E
     */
    void Update();

    /**
     * @brief レターボックスの描画
     * 
     * 現在の状態に応じてレターボックスを描画します、E
     * 完�Eに非表示の場合�E描画処琁E��スキチE�Eします、E
     */
    void Draw();

    /**
     * @brief 現在の状態を取征E
     * @return 現在のレターボックスの状慁E
     */
    LetterboxState GetState() const { return state_; }

    /**
     * @brief イージングタイプを設宁E
     * @param type 適用するイージングタイチE
     */
    void SetEaseType(LetterboxEase type) { easeType_ = type; }
    
    /**
     * @brief レターボックスの高さを設宁E
     * 
     * 画面上下それぞれ�Eレターボックスの高さを変更します、E
     * サイズと位置が即座に更新されます、E
     * 
     * @param height レターボックスの高さ�E�ピクセル�E�E
     */
    void SetLetterboxHeight(float height);
    
    /**
     * @brief レターボックスの色を設宁E
     * @param color 設定する色�E�EGBA�E�E
     */
    void SetColor(const Vector4& color);

    /**
     * @brief ImGuiチE��チE��ウィンドウの表示
     * 
     * 状態、E��行度、パラメータ調整などをImGuiで表示します、E
     * USE_IMGUIマクロが定義されてぁE��場合�Eみ有効です、E
     */
    void ShowImGui();

private:
    /**
     * @brief イージング関数を適用
     * 
     * 設定されたイージングタイプに基づぁE��、補間値を計算します、E
     * 
     * @param t 正規化された時間！E.0、E.0�E�E
     * @return イージングを適用した値�E�E.0、E.0、オーバ�Eシュート型は篁E��外も可�E�E
     */
    float ApplyEasing(float t) const;
    
    /**
     * @brief バ�Eの位置を更新
     * 
     * 現在の進行度に基づぁE��、上下�Eバ�EのY座標を計算して更新します、E
     */
    void UpdateBarPositions();
    
    /**
     * @brief バ�Eのサイズを更新
     * 
     * レターボックスの高さとオーバ�Eシュート�Eージンを老E�Eしてサイズを更新します、E
     */
    void UpdateBarSizes();

private:
    float screenWidth_ = 1280.0f;   ///< 画面幁E��ピクセル�E�E
    float screenHeight_ = 720.0f;   ///< 画面高さ�E�ピクセル�E�E

    float letterboxHeight_ = 100.0f; ///< レターボックスの高さ�E�画面の上下それぞれ！E

    float overshootMargin_ = 100.0f; ///< オーバ�Eシュート用の余白�E�EackやElasticイージングで篁E��外に出る�E�E�E

    float duration_ = 1.0f;  ///< アニメーション時間�E�秒！E
    float elapsed_ = 0.0f;   ///< 経過時間�E�秒！E
    float progress_ = 0.0f;  ///< アニメーション進行度�E�E.0=完�Eに隠れてぁE��, 1.0=完�Eに表示�E�E

    LetterboxState state_ = LetterboxState::Hidden;            ///< 現在の状慁E
    LetterboxEase easeType_ = LetterboxEase::InOutBack;        ///< 適用するイージングタイチE

    Vector4 color_ = { 0.0f, 0.0f, 0.0f, 1.0f }; ///< バ�Eの色�E�デフォルト�E黒！E

    std::unique_ptr<Sprite> topBar_;    ///< 上部のレターボックスバ�E
    std::unique_ptr<Sprite> bottomBar_; ///< 下部のレターボックスバ�E
};
