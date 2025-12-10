#pragma once
#include <vector>
#include <memory>
#include <string>
#include "graphics/2d/Sprite.h"
#include "math/Easing.h"

/**
 * @brief 繧ｷ繝ｼ繝ｳ驕ｷ遘ｻ縺ｮ譁ｹ蜷代ヱ繧ｿ繝ｼ繝ｳ
 * 
 * 繧ｰ繝ｪ繝・ラ迥ｶ縺ｮ繧ｹ繝励Λ繧､繝医′縺ｩ縺ｮ譁ｹ蜷代°繧蛾・↓螟牙喧縺励※縺・￥縺九ｒ螳夂ｾｩ縺励∪縺吶・
 */
enum class TransitionMode
{
    LeftTopToRightBottom,      ///< 蟾ｦ荳翫°繧牙承荳九∈驕ｷ遘ｻ
    RightBottomToLeftTop,      ///< 蜿ｳ荳九°繧牙ｷｦ荳翫∈驕ｷ遘ｻ
    RightTopToLeftBottom,      ///< 蜿ｳ荳翫°繧牙ｷｦ荳九∈驕ｷ遘ｻ
    LeftBottomToRightTop,      ///< 蟾ｦ荳九°繧牙承荳翫∈驕ｷ遘ｻ
    TopToBottom,               ///< 荳翫°繧我ｸ九∈驕ｷ遘ｻ
    BottomToTop,               ///< 荳九°繧我ｸ翫∈驕ｷ遘ｻ
    CenterToEdges,             ///< 荳ｭ螟ｮ縺九ｉ螟門・縺ｸ驕ｷ遘ｻ
    EdgesToCenter              ///< 螟門・縺九ｉ荳ｭ螟ｮ縺ｸ驕ｷ遘ｻ
};

/**
 * @brief 繝輔ぉ繝ｼ繝峨・遞ｮ鬘・
 */
enum class FadeType
{
    FadeIn,     ///< 繝輔ぉ繝ｼ繝峨う繝ｳ・磯乗・竊剃ｸ埼乗・・・
    FadeOut     ///< 繝輔ぉ繝ｼ繝峨い繧ｦ繝茨ｼ井ｸ埼乗・竊帝乗・・・
};

/**
 * @brief 繧ｷ繝ｼ繝ｳ驕ｷ遘ｻ縺ｮ繧､繝ｼ繧ｸ繝ｳ繧ｰ繧ｿ繧､繝・
 * 
 * 繧ｰ繝ｪ繝・ラ縺ｮ騾乗・蠎ｦ螟牙喧縺ｫ驕ｩ逕ｨ縺吶ｋ繧､繝ｼ繧ｸ繝ｳ繧ｰ髢｢謨ｰ縺ｮ遞ｮ鬘槭ｒ螳夂ｾｩ縺励∪縺吶・
 */
enum class SceneTransitionEase
{
    Linear,         ///< 邱壼ｽ｢陬憺俣
    InSine,         ///< 繧ｵ繧､繝ｳ蜉騾・
    OutSine,        ///< 繧ｵ繧､繝ｳ貂幃・
    InOutSine,      ///< 繧ｵ繧､繝ｳ蜉騾滓ｸ幃・
    InQuint,        ///< 5荵怜刈騾・
    OutQuint,       ///< 5荵玲ｸ幃・
    InOutQuint,     ///< 5荵怜刈騾滓ｸ幃・
    InCirc,         ///< 蜀・ｽ｢蜉騾・
    OutCirc,        ///< 蜀・ｽ｢貂幃・
    InOutCirc,      ///< 蜀・ｽ｢蜉騾滓ｸ幃・
    InElastic,      ///< 蠑ｾ諤ｧ蜉騾・
    OutElastic,     ///< 蠑ｾ諤ｧ貂幃・
    InOutElastic,   ///< 蠑ｾ諤ｧ蜉騾滓ｸ幃・
    InExpo,         ///< 謖・焚蜉騾・
    OutExpo,        ///< 謖・焚貂幃・
    InOutExpo,      ///< 謖・焚蜉騾滓ｸ幃・
    OutQuad,        ///< 2荵玲ｸ幃・
    InOutQuart,     ///< 4荵怜刈騾滓ｸ幃・
    InBack,         ///< 蠕碁蜉騾・
    OutBack,        ///< 蠕碁貂幃・
    InOutBack,      ///< 蠕碁蜉騾滓ｸ幃・
    OutBounce,      ///< 繝舌え繝ｳ繝画ｸ幃・
    InBounce,       ///< 繝舌え繝ｳ繝牙刈騾・
    InOutBounce     ///< 繝舌え繝ｳ繝牙刈騾滓ｸ幃・
};

/**
 * @brief 驕ｷ遘ｻ繧ｨ繝輔ぉ繧ｯ繝医・迥ｶ諷・
 */
enum class TransitionState { 
    Idle,       ///< 蠕・ｩ滉ｸｭ・亥・逕溷燕・・
    Playing,    ///< 蜀咲函荳ｭ
    Done        ///< 螳御ｺ・
};

/**
 * @brief 繧ｷ繝ｼ繝ｳ驕ｷ遘ｻ繧ｨ繝輔ぉ繧ｯ繝医け繝ｩ繧ｹ
 * 
 * 逕ｻ髱｢蜈ｨ菴薙ｒ繧ｰ繝ｪ繝・ラ迥ｶ縺ｫ蛻・牡縺励∝推繧ｻ繝ｫ縺碁・ｬ｡繝輔ぉ繝ｼ繝峨う繝ｳ/繧｢繧ｦ繝医☆繧九％縺ｨ縺ｧ
 * 隕冶ｦ夂噪縺ｫ鄒弱＠縺・す繝ｼ繝ｳ驕ｷ遘ｻ貍泌・繧貞ｮ溽樟縺励∪縺吶・
 * 
 * 荳ｻ縺ｪ讖溯・:
 * - 繧ｫ繧ｹ繧ｿ繝槭う繧ｺ蜿ｯ閭ｽ縺ｪ繧ｰ繝ｪ繝・ラ繧ｵ繧､繧ｺ
 * - 8遞ｮ鬘槭・驕ｷ遘ｻ譁ｹ蜷代ヱ繧ｿ繝ｼ繝ｳ
 * - 繧ｰ繝ｩ繝・・繧ｷ繝ｧ繝ｳ繧ｫ繝ｩ繝ｼ蟇ｾ蠢・
 * - 螟壽ｧ倥↑繧､繝ｼ繧ｸ繝ｳ繧ｰ髢｢謨ｰ
 * - 繝輔ぉ繝ｼ繝峨う繝ｳ/繝輔ぉ繝ｼ繝峨い繧ｦ繝亥・繧頑崛縺・
 * 
 * @code
 * // 菴ｿ逕ｨ萓・
 * SceneTransitionEffect transition;
 * transition.Initialize(spriteCommon, texPath, 6, 4, 1280.0f, 720.0f);
 * transition.SetMode(TransitionMode::CenterToEdges);
 * transition.SetFadeType(FadeType::FadeOut);
 * transition.Start(1.5f, {1,1,1,1}, {0,0,0,1});  // 逋ｽ縺九ｉ鮟偵∈繧ｰ繝ｩ繝・・繧ｷ繝ｧ繝ｳ
 * @endcode
 */
class SceneTransitionEffect
{
public:
    SceneTransitionEffect();
    ~SceneTransitionEffect();

    /**
     * @brief 繧ｨ繝輔ぉ繧ｯ繝医・蛻晄悄蛹・
     * 
     * 繧ｰ繝ｪ繝・ラ迥ｶ縺ｮ繧ｹ繝励Λ繧､繝医ｒ逕滓・縺励∫判髱｢蜈ｨ菴薙ｒ隕・≧繧医≧縺ｫ驟咲ｽｮ縺励∪縺吶・
     * 
     * @param spriteCommon 繧ｹ繝励Λ繧､繝亥・騾夊ｨｭ螳・
     * @param texturePath 菴ｿ逕ｨ縺吶ｋ繝・け繧ｹ繝√Ε繝代せ
     * @param gridX 讓ｪ譁ｹ蜷代・繧ｰ繝ｪ繝・ラ蛻・牡謨ｰ
     * @param gridY 邵ｦ譁ｹ蜷代・繧ｰ繝ｪ繝・ラ蛻・牡謨ｰ
     * @param screenWidth 逕ｻ髱｢蟷・ｼ医ヴ繧ｯ繧ｻ繝ｫ・・
     * @param screenHeight 逕ｻ髱｢鬮倥＆・医ヴ繧ｯ繧ｻ繝ｫ・・
     */
    void Initialize(SpriteCommon* spriteCommon, const std::string& texturePath, int gridX, int gridY, float screenWidth, float screenHeight);

    /**
     * @brief 繧ｰ繝ｩ繝・・繧ｷ繝ｧ繝ｳ繧ｫ繝ｩ繝ｼ莉倥″驕ｷ遘ｻ縺ｮ髢句ｧ・
     * 
     * 髢句ｧ玖牡縺九ｉ邨ゆｺ・牡縺ｸ縺ｮ繧ｰ繝ｩ繝・・繧ｷ繝ｧ繝ｳ繧帝←逕ｨ縺励◆驕ｷ遘ｻ繧ｨ繝輔ぉ繧ｯ繝医ｒ髢句ｧ九＠縺ｾ縺吶・
     * 
     * @param duration 驕ｷ遘ｻ譎る俣・育ｧ抵ｼ・
     * @param startColor 髢句ｧ区凾縺ｮ濶ｲ・・GBA・・
     * @param endColor 邨ゆｺ・凾縺ｮ濶ｲ・・GBA・・
     */
    void Start(float duration, const Vector4& startColor, const Vector4& endColor);

    /**
     * @brief 繧ｨ繝輔ぉ繧ｯ繝医・譖ｴ譁ｰ
     * 
     * 豈弱ヵ繝ｬ繝ｼ繝蜻ｼ縺ｳ蜃ｺ縺輔ｌ縲√げ繝ｪ繝・ラ縺ｮ騾乗・蠎ｦ繧呈峩譁ｰ縺励∪縺吶・
     */
    void Update();
    
    /**
     * @brief 繧ｨ繝輔ぉ繧ｯ繝医・謠冗判
     * 
     * 蜈ｨ縺ｦ縺ｮ繧ｰ繝ｪ繝・ラ繧ｹ繝励Λ繧､繝医ｒ謠冗判縺励∪縺吶・
     */
    void Draw();

    /**
     * @brief 迴ｾ蝨ｨ縺ｮ迥ｶ諷九ｒ蜿門ｾ・
     * @return 驕ｷ遘ｻ繧ｨ繝輔ぉ繧ｯ繝医・迥ｶ諷・
     */
    TransitionState GetState() const;
    
    /**
     * @brief 迥ｶ諷九ｒ險ｭ螳・
     * @param state 險ｭ螳壹☆繧狗憾諷・
     */
	void SetState(TransitionState state);
    
    /**
     * @brief 繧､繝ｼ繧ｸ繝ｳ繧ｰ繧ｿ繧､繝励ｒ險ｭ螳・
     * @param type 驕ｩ逕ｨ縺吶ｋ繧､繝ｼ繧ｸ繝ｳ繧ｰ繧ｿ繧､繝・
     */
    void SetEaseType(SceneTransitionEase type);
    
    /**
     * @brief 驕ｷ遘ｻ繝｢繝ｼ繝峨ｒ險ｭ螳・
     * @param mode 驕ｷ遘ｻ縺ｮ譁ｹ蜷代ヱ繧ｿ繝ｼ繝ｳ
     */
    void SetMode(TransitionMode mode);
    
    /**
     * @brief 繝輔ぉ繝ｼ繝峨ち繧､繝励ｒ險ｭ螳・
     * @param type 繝輔ぉ繝ｼ繝峨う繝ｳ/繝輔ぉ繝ｼ繝峨い繧ｦ繝医・驕ｸ謚・
     */
    void SetFadeType(FadeType type);

    /**
     * @brief ImGui繝・ヰ繝・げ繧ｦ繧｣繝ｳ繝峨え縺ｮ陦ｨ遉ｺ
     */
    void ShowImGui();

private:
    /**
     * @brief 繧､繝ｼ繧ｸ繝ｳ繧ｰ髢｢謨ｰ繧帝←逕ｨ
     * @param t 豁｣隕丞喧縺輔ｌ縺滓凾髢難ｼ・.0縲・.0・・
     * @return 繧､繝ｼ繧ｸ繝ｳ繧ｰ驕ｩ逕ｨ蠕後・蛟､
     */
    float ApplyEasing(float t) const;
    
    /**
     * @brief 濶ｲ縺ｮ邱壼ｽ｢陬憺俣
     * @param c0 髢句ｧ玖牡
     * @param c1 邨ゆｺ・牡
     * @param t 陬憺俣菫よ焚・・.0縲・.0・・
     * @return 陬憺俣縺輔ｌ縺溯牡
     */
    Vector4 LerpColor(const Vector4& c0, const Vector4& c1, float t) const;
    
    /**
     * @brief 繧ｰ繝ｪ繝・ラ菴咲ｽｮ縺ｫ蝓ｺ縺･縺城ｲ陦悟ｺｦ繧定ｨ育ｮ・
     * 
     * 驕ｷ遘ｻ繝｢繝ｼ繝峨↓蠢懊§縺ｦ縲∝推繧ｰ繝ｪ繝・ラ繧ｻ繝ｫ縺ｮ螟牙喧髢句ｧ九ち繧､繝溘Φ繧ｰ繧呈ｱｺ螳壹＠縺ｾ縺吶・
     * 
     * @param x 繧ｰ繝ｪ繝・ラ縺ｮX蠎ｧ讓・
     * @param y 繧ｰ繝ｪ繝・ラ縺ｮY蠎ｧ讓・
     * @return 繧ｰ繝ｪ繝・ラ騾ｲ陦悟ｺｦ・・.0縲・.0・・
     */
    float CalcGridProgress(int x, int y) const;

    int gridX_ = 6;                                         ///< 讓ｪ譁ｹ蜷代・繧ｰ繝ｪ繝・ラ蛻・牡謨ｰ
    int gridY_ = 4;                                         ///< 邵ｦ譁ｹ蜷代・繧ｰ繝ｪ繝・ラ蛻・牡謨ｰ
    float screenWidth_ = 1280.0f;                           ///< 逕ｻ髱｢蟷・
    float screenHeight_ = 720.0f;                           ///< 逕ｻ髱｢鬮倥＆
    float transitionRate_ = 0.0f;                           ///< 蜈ｨ菴薙・驕ｷ遘ｻ騾ｲ陦悟ｺｦ・・.0縲・.0・・
    SceneTransitionEase easeType_ = SceneTransitionEase::Linear;  ///< 繧､繝ｼ繧ｸ繝ｳ繧ｰ繧ｿ繧､繝・
    float duration_ = 1.0f;                                 ///< 驕ｷ遘ｻ譎る俣・育ｧ抵ｼ・
    float elapsed_ = 0.0f;                                  ///< 邨碁℃譎る俣・育ｧ抵ｼ・
    TransitionState state_ = TransitionState::Idle;         ///< 迴ｾ蝨ｨ縺ｮ迥ｶ諷・
    Vector4 startColor_ = { 1.0f,1.0f,1.0f,1.0f };          ///< 髢句ｧ区凾縺ｮ濶ｲ
    Vector4 endColor_ = { 1.0f,1.0f,1.0f,1.0f };            ///< 邨ゆｺ・凾縺ｮ濶ｲ
    TransitionMode mode_ = TransitionMode::LeftTopToRightBottom;  ///< 驕ｷ遘ｻ繝｢繝ｼ繝・
    FadeType fadeType_ = FadeType::FadeOut;                 ///< 繝輔ぉ繝ｼ繝峨ち繧､繝・

    std::vector<std::vector<std::unique_ptr<Sprite>>> gridSprites_;  ///< 繧ｰ繝ｪ繝・ラ迥ｶ縺ｫ驟咲ｽｮ縺輔ｌ縺溘せ繝励Λ繧､繝育ｾ､
};
