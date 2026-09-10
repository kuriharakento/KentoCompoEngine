#pragma once
#include <string>
#include <vector>

#include "graphics/pipeline/IRenderPass.h"

namespace KCE
{
/**
 * @brief 差し替え可能なパスの列として描画を組み立てるクラス
 *
 * @details SEQUENCER_PLAN 4.2。今後ボリュメトリック、DoF、TAA、半透明パス、
 *          モニター用サブビュー、トランジション、レターボックスを
 *          足していくことになるが、直列コードのままでは確実に破綻する。
 *
 *          パスの順序がそのまま描画順になる。
 *          新しいパスは AddPass / InsertPass で差し込む。
 */
class RenderPipeline
{
public:
	/**
	 * @brief パスを末尾に追加する
	 * @param pass 追加するパス
	 * @return 追加されたパスへのポインタ。nullptrを渡した場合はnullptr
	 */
	IRenderPass* AddPass(RenderPassPtr pass);

	/**
	 * @brief 指定位置にパスを挿入する
	 * @param index 挿入位置。範囲を超える場合は末尾へ追加される
	 * @param pass 挿入するパス
	 * @return 挿入されたパスへのポインタ
	 */
	IRenderPass* InsertPass(size_t index, RenderPassPtr pass);

	/**
	 * @brief 名前で指定したパスの直後に挿入する
	 * @details 「フォワードパスの後にビームを足す」のように、
	 *          インデックスではなく前後関係で指定できるようにする。
	 * @param passName 基準にするパスの名前
	 * @param pass 挿入するパス
	 * @return 挿入されたパスへのポインタ。基準が見つからない場合は末尾へ追加
	 */
	IRenderPass* InsertPassAfter(const std::string& passName, RenderPassPtr pass);

	/**
	 * @brief 名前でパスを探す
	 * @param passName パスの名前
	 * @return 見つかったパス。無ければnullptr
	 */
	IRenderPass* FindPass(const std::string& passName) const;

	/**
	 * @brief 名前でパスを取り除く
	 * @param passName パスの名前
	 * @return 取り除いた場合は真
	 */
	bool RemovePass(const std::string& passName);

	/**
	 * @brief 登録順にパスを実行する
	 * @details コンテキストが不足している場合は何もしない。
	 * @param ctx 描画コンテキスト
	 */
	void Execute(const RenderPassContext& ctx);

	/**
	 * @brief 全パスを破棄する
	 */
	void Clear();

	size_t GetPassCount() const { return passes_.size(); }
	const std::vector<RenderPassPtr>& GetPasses() const { return passes_; }

#ifdef USE_IMGUI
	/**
	 * @brief デバッグUIを登録する
	 */
	void RegisterDebugUI();

	/**
	 * @brief パスの一覧と有効・無効の切り替えを描画する
	 */
	void DrawImGui();
#endif

	~RenderPipeline();

private:
	// 描画順に並んだパス
	std::vector<RenderPassPtr> passes_;
};
} // namespace KCE
