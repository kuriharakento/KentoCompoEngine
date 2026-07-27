#pragma once
#include "base/DirectXCommon.h"

namespace KCE
{
/**
 * @brief モデル共通部クラス
 * @details 3Dモデル描画に必要なDirectXCommonへのポインタを保持する
 */
class ModelCommon
{
public:
	/**
	 * @brief 初期化
	 * @param dxCommon DirectXCommonへのポインタ
	 */
	void Initialize(DirectXCommon* dxCommon);

public: // アクセッサ
	/**
	 * @brief DirectXCommonの取得
	 * @return DirectXCommonへのポインタ
	 */
	DirectXCommon* GetDXCommon() { return dxCommon_; }

private:
	// DirectXCommonへのポインタ
	DirectXCommon* dxCommon_;

};
} // namespace KCE
