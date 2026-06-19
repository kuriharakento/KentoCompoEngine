#pragma once
#include <cstdint>

/**
 * @brief ライトの最大数を定義する名前空間
 * 
 * GPU側のシェーダーで使用するライト配列のサイズ上限を定義します。
 * この値を変更する場合は、対応するシェーダーコードも更新が必要です。
 */
namespace LightMaxCount
{
	// ポイントライトの最大数
	constexpr uint32_t kMaxPointLightCount = 100;
	// スポットライトの最大数
	constexpr uint32_t kMaxSpotLightCount = 100;
}

/**
 * @brief ライトのカウント情報
 * 
 * GPU側の定数バッファとして使用するライトカウント構造体。
 * シェーダーでのループ回数制御に使用されます。
 */
struct LightCount
{
	uint32_t pointLightCount = 0; // 現在のポイントライト数
	uint32_t spotLightCount = 0;  // 現在のスポットライト数
	uint32_t padding;             // 16バイトアライメント用パディング
};
