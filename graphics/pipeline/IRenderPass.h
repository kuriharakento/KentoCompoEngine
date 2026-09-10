#pragma once
#include <memory>

#include "graphics/pipeline/RenderPassContext.h"

namespace KCE
{
/**
 * @brief 1つの描画パス
 *
 * @details パスは「入力として何を読み、出力として何に描くか」を
 *          RenderPassContext から受け取り、自分の描画だけを行う。
 *          他のパスの存在を前提にしないこと。前後の並び替えや
 *          差し替えができなくなる。
 *
 *          リソースバリアは、そのリソースを所有するパスが張る。
 *          今はまだ手動だが、パスの入出力が宣言的に整理されたので、
 *          後から自動化する余地は残してある。
 */
class IRenderPass
{
public:
	virtual ~IRenderPass() = default;

	/**
	 * @brief パスの名前
	 * @details エディタでの一覧表示と、GPUマーカーに使う。
	 * @return 名前
	 */
	virtual const char* GetName() const = 0;

	/**
	 * @brief パスを実行する
	 * @param ctx 描画に必要なものをまとめたコンテキスト
	 */
	virtual void Execute(const RenderPassContext& ctx) = 0;

	/**
	 * @brief このフレームで実行すべきか
	 *
	 * @details 既定では有効フラグだけを見る。
	 *          「出力先がレンダーターゲットのときだけ動く」といった
	 *          条件を持つパスは、ここを上書きする。
	 *
	 * @param ctx 描画コンテキスト
	 * @return 実行するなら真
	 */
	virtual bool ShouldExecute(const RenderPassContext& ctx) const
	{
		(void)ctx;
		return enabled_;
	}

	/**
	 * @brief パスの有効・無効を設定する
	 * @details 絵の切り分けのために、エディタから個別に落とせるようにしてある。
	 * @param enabled 有効にするなら真
	 */
	void SetEnabled(bool enabled) { enabled_ = enabled; }
	bool IsEnabled() const { return enabled_; }

protected:
	// パスが有効かどうか
	bool enabled_ = true;
};

using RenderPassPtr = std::unique_ptr<IRenderPass>;
} // namespace KCE
