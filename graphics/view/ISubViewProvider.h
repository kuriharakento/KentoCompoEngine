#pragma once
#include <cstdint>
#include <string>

namespace KCE
{
class RenderView;

/**
 * @brief シーン側からサブビュー（別カメラの描画先）を作ってもらう窓口
 *
 * - 作ったビューは毎フレーム、本編の後・ポストプロセスの前に描かれる
 * - 所有は実装側（Framework）。シーンは返されたポインタを借りるだけ
 */
class ISubViewProvider
{
public:
	virtual ~ISubViewProvider() = default;

	/**
	 * @brief サブビューを作って毎フレームの描画に登録する
	 * @param name デバッグ用の名前
	 * @param width 幅（ピクセル）
	 * @param height 高さ（ピクセル）
	 * @return 作ったビュー（所有しない）。DestroySubView まで有効
	 */
	virtual RenderView* CreateSubView(const std::string& name, uint32_t width, uint32_t height) = 0;

	/**
	 * @brief CreateSubView で作ったビューを登録解除して破棄する
	 * @details 更新中に呼ぶ前提。前フレームの GPU 処理は PostDraw で待ち終わっている。
	 * @param view 破棄するビュー。nullptr や知らないビューは無視する
	 */
	virtual void DestroySubView(RenderView* view) = 0;
};
} // namespace KCE
