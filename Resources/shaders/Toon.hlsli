// トゥーン（NPR）シェーディングの共通関数
// SEQUENCER_PLAN 7.1。ライトパス（ディファード）とObject3d（フォワード）で同じ計算を使う。
//
// 物理的に正しいライティングをそのままアニメ調の肌に当てると、陰影の階調が多すぎて
// 汚く見える。明暗を少数の段に塗り分け、境界だけを柔らかくする。

#ifndef TOON_HLSLI
#define TOON_HLSLI

// 全体設定の既定値。フォワード描画はルートシグネチャを変えずに済むよう、この値を使う。
// ディファード描画は ToonSettings の cbuffer で上書きできる。
static const float kDefaultToonThreshold = 0.5f;
static const float kDefaultToonSoftness = 0.05f;
static const float3 kDefaultToonShadowTint = float3(0.55f, 0.5f, 0.65f);
static const float3 kDefaultRimColor = float3(1.0f, 1.0f, 1.0f);
static const float kDefaultRimPower = 4.0f;

// 陰影の塗り分け
// lambert : 0〜1 の拡散の強さ（NdotL）
// 戻り値  : 明部 1 / 暗部 0 の2段。境界だけ softness の幅で滑らかにつなぐ
float ToonRamp(float lambert, float threshold, float softness)
{
    return smoothstep(threshold - softness, threshold + softness, lambert);
}

// 拡散光の色を、従来のライティングとトゥーンで混ぜる
// litColor   : 光源の色 × 強さ × 減衰 × 影 を含まない、ライトの色
// albedo     : 素材の色
// lambert    : NdotL（0〜1）
// shadow     : シャドウマップの結果（0〜1）
// toonAmount : 0 で従来、1 で完全なトゥーン
float3 ToonDiffuse(float3 albedo, float3 lightColor, float lambert, float shadow, float toonAmount,
                   float threshold, float softness, float3 shadowTint)
{
    // 従来: 拡散の強さにそのまま比例
    float3 standard = albedo * lightColor * lambert * shadow;

    // トゥーン: シャドウマップの影もランプの暗部として同じ色で塗る。
    // 影を黒で落とすと、アニメ調の絵では濁って見えるため、暗部は影色を乗算した素材色にする。
    float lit = ToonRamp(lambert * shadow, threshold, softness);
    float3 toon = albedo * lightColor * lerp(shadowTint, float3(1.0f, 1.0f, 1.0f), lit);

    return lerp(standard, toon, saturate(toonAmount));
}

// リムライト（輪郭が光る表現）
// normal / toEye : ワールド空間の単位ベクトル
// rimStrength    : 0 で無効
float3 RimLight(float3 normal, float3 toEye, float rimStrength, float3 rimColor, float rimPower)
{
    float rim = pow(saturate(1.0f - dot(normal, toEye)), rimPower);
    return rimColor * rim * saturate(rimStrength);
}

#endif // TOON_HLSLI
