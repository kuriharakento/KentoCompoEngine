#pragma once
#include <cstdint>

namespace KCE
{
/**
 * @brief 描画対象を絞り込むためのレイヤーマスク
 *
 * @details ビューごとに「何を描くか」を変えるために使う。
 *          オブジェクトは所属レイヤーの集合をマスクで持ち、
 *          ビューは描きたいレイヤーの集合をマスクで持つ。
 *          両者のビット積が0でないものだけが描かれる。
 *
 *          例:
 *          - ステージモニターへの中継映像は、キャラクターだけを描いて負荷を抑える
 *          - キャラクタープレビューは、そのキャラクターと専用ライトだけを描く
 *          - ミニマップは地形とアイコンだけを描く
 */
using RenderLayerMask = uint32_t;

/** @brief 既定レイヤーの番号 */
constexpr uint32_t kRenderLayerDefaultIndex = 0;

/** @brief ステージモニターの画面用レイヤーの番号 */
constexpr uint32_t kRenderLayerMonitorScreenIndex = 1;

/** @brief 平面反射する面用レイヤーの番号 */
constexpr uint32_t kRenderLayerReflectorIndex = 2;

/** @brief アプリ用レイヤーの開始番号。0〜7はエンジン用に予約する */
constexpr uint32_t kRenderLayerFirstAppIndex = 8;

/** @brief すべてのレイヤーを描く */
constexpr RenderLayerMask kRenderLayerAll = 0xFFFFFFFFu;

/** @brief 何も描かない */
constexpr RenderLayerMask kRenderLayerNone = 0u;

/**
 * @brief レイヤー番号からマスクを作る
 * @param index レイヤー番号（0〜31）
 * @return マスク
 */
constexpr RenderLayerMask MakeRenderLayerMask(uint32_t index)
{
	return 1u << (index & 31u);
}

/** @brief 既定のレイヤー。指定しないオブジェクトはすべてここに入る */
constexpr RenderLayerMask kRenderLayerDefault = MakeRenderLayerMask(kRenderLayerDefaultIndex);

/** @brief ステージモニターの画面用レイヤー */
constexpr RenderLayerMask kRenderLayerMonitorScreen = MakeRenderLayerMask(kRenderLayerMonitorScreenIndex);

/** @brief 平面反射する面用レイヤー */
constexpr RenderLayerMask kRenderLayerReflector = MakeRenderLayerMask(kRenderLayerReflectorIndex);

/**
 * @brief アプリでの使い方
 *
 * レイヤー番号は kRenderLayerFirstAppIndex から31までを使う。
 * MakeRenderLayerMask で番号をマスクへ変換し、GameObject::AddRenderLayer で物に足す。
 * RenderView::SetLayerMask には、描きたいマスクをビット和で渡す。
 * 番号は0〜31、マスクは複数の所属を表すビット集合なので混同しない。
 */

/**
 * @brief オブジェクトがそのビューに描かれるか
 * @param objectLayer オブジェクトの所属レイヤー
 * @param viewMask ビューが描くレイヤーの集合
 * @return 描かれるなら真
 */
constexpr bool IsVisibleInLayerMask(RenderLayerMask objectLayer, RenderLayerMask viewMask)
{
	return (objectLayer & viewMask) != 0;
}
} // namespace KCE
