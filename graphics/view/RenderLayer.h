#pragma once
#include <cstdint>

namespace KCE
{
/**
 * @brief 描画対象を絞り込むためのレイヤーマスク
 *
 * @details ビューごとに「何を描くか」を変えるために使う。
 *          オブジェクトは自分の所属レイヤーを1つ持ち、
 *          ビューは描きたいレイヤーの集合をマスクで持つ。
 *          両者のビット積が0でないものだけが描かれる。
 *
 *          例:
 *          - ステージモニターへの中継映像は、キャラクターだけを描いて負荷を抑える
 *          - キャラクタープレビューは、そのキャラクターと専用ライトだけを描く
 *          - ミニマップは地形とアイコンだけを描く
 */
using RenderLayerMask = uint32_t;

/** @brief 既定のレイヤー。指定しないオブジェクトはすべてここに入る */
constexpr RenderLayerMask kRenderLayerDefault = 1u << 0;

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
