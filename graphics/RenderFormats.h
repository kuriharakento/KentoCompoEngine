#pragma once
#include <dxgiformat.h>

namespace KCE
{
/**
 * @brief 描画ターゲットのフォーマット定義
 *
 * @details PSOのRTVフォーマットは、実際に描くレンダーターゲットのフォーマットと
 *          一致していなければならない。パスごとに数値を直書きすると、
 *          フォーマットを変えたときに片方だけ直し忘れて描画が壊れる。
 *          シーンに描くパスは必ずここの定数を使うこと。
 */

/**
 * @brief シーンを描くレンダーターゲットのフォーマット
 *
 * @details SEQUENCER_PLAN 4.1。以前は R8G8B8A8_UNORM_SRGB だったため、
 *          ライトパスの出力が 1.0 でクリップされ、ステージ照明の強い発光や
 *          ビームがブライトパスへ渡る前に潰れていた。
 *          HDRのまま保持し、ポストプロセスの最終段でトーンマップしてLDRへ落とす。
 */
constexpr DXGI_FORMAT kSceneColorFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

/**
 * @brief 画面に出す最終出力のフォーマット
 * @details トーンマップ後のLDR。sRGBフォーマットなので、
 *          書き込む値はリニアのままでよい（ハードウェアがsRGBへ変換する）。
 */
constexpr DXGI_FORMAT kDisplayColorFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

/**
 * @brief ブルームの中間バッファのフォーマット
 */
constexpr DXGI_FORMAT kBloomBufferFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
} // namespace KCE
