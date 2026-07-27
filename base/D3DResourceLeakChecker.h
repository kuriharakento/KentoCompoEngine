#pragma once

namespace KCE
{
/**
 * @brief Direct3Dリソースリークチェッカークラス
 */
class D3DResourceLeakChecker
{
public:
	/**
	 * @brief デストラクタ
	 * 
	 * 解放されていないDirect3Dリソースをデバッグ出力に報告する
	 */
	~D3DResourceLeakChecker();
};
} // namespace KCE
