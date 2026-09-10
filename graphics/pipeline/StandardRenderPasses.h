#pragma once
#include <functional>
#include <utility>

#include "graphics/pipeline/IRenderPass.h"
#include "graphics/pipeline/RenderPipeline.h"

namespace KCE
{
/**
 * @brief 3D描画の共通設定を行う
 *
 * @details 3Dオブジェクトの共通ルートシグネチャ設定と、
 *          カスケードシャドウマップのSRV／CBVのバインドを行う。
 *          フォワードパスの直前に必要になるが、
 *          アプリ側から明示的に呼びたい場面もあるため外に出してある。
 *
 * @param ctx 描画コンテキスト
 */
void ApplyCommon3DRenderingSetting(const RenderPassContext& ctx);

/**
 * @brief 2D描画の共通設定を行う
 * @param ctx 描画コンテキスト
 */
void ApplyCommon2DRenderingSetting(const RenderPassContext& ctx);

/**
 * @brief シャドウマップ生成パス
 *
 * @details カスケード4枚、スポットライト、ポイントライト（6面）を描く。
 *          ライト数ぶんシーン全体を再描画するため、ライトを増やすと
 *          ここが真っ先に重くなる。上限設定・更新頻度の間引き・
 *          キャスタのカリングは、性能問題が出てから対処すればよい。
 */
class ShadowMapPass : public IRenderPass
{
public:
	const char* GetName() const override { return "ShadowMap"; }
	void Execute(const RenderPassContext& ctx) override;
};

/**
 * @brief G-Buffer生成パス
 */
class GBufferPass : public IRenderPass
{
public:
	const char* GetName() const override { return "GBuffer"; }
	void Execute(const RenderPassContext& ctx) override;
};

/**
 * @brief ライトパス
 * @details シーン用レンダーターゲットへの描画をここで開始する。
 *          以降のフォワード系パスは同じターゲットへ描き足していく。
 */
class LightingPass : public IRenderPass
{
public:
	const char* GetName() const override { return "Lighting"; }
	void Execute(const RenderPassContext& ctx) override;
};

/**
 * @brief フォワード描画パス
 * @details 深度バッファを書き込み可能に戻し、シーン用レンダーターゲットと
 *          G-Bufferの深度を束ねてから、フォワード対象とデバッグラインを描く。
 */
class ForwardPass : public IRenderPass
{
public:
	const char* GetName() const override { return "Forward"; }
	void Execute(const RenderPassContext& ctx) override;
};

/**
 * @brief Skybox描画パス
 */
class SkyboxPass : public IRenderPass
{
public:
	const char* GetName() const override { return "Skybox"; }
	void Execute(const RenderPassContext& ctx) override;
};

/**
 * @brief パーティクル描画パス
 * @details 現状は半透明のソートを持たないため、常に最後に描かれる。
 *          ビーム・煙・ガラスが同居する演出では破綻するので、
 *          不透明パスと半透明パスの分離が別途必要になる。
 */
class ParticlePass : public IRenderPass
{
public:
	const char* GetName() const override { return "Particle"; }
	void Execute(const RenderPassContext& ctx) override;
};

/**
 * @brief シーン用レンダーターゲットへの描画を終えるパス
 * @details 深度をSRV状態へ戻し、シーン用レンダーターゲットを
 *          シェーダーリソースとして読める状態にする。
 *          新しいフォワード系パスは、必ずこれより前に挿入すること。
 */
class SceneColorResolvePass : public IRenderPass
{
public:
	const char* GetName() const override { return "SceneColorResolve"; }
	void Execute(const RenderPassContext& ctx) override;
};

/**
 * @brief 登録されたサブビューを描くパス
 *
 * @details 中継映像やカメラプレビューは、本編のシャドウマップを共有するため
 *          シャドウパスより後に描く必要がある。一方でポストプロセスより前に
 *          置かないと、レンダーターゲットの状態を踏み荒らしてしまう。
 *          その両方を満たす位置がここになる。
 *
 *          実際の描画は Framework が持つため、コールバックとして受け取る。
 */
class SubViewRenderPass : public IRenderPass
{
public:
	const char* GetName() const override { return "SubViews"; }
	void Execute(const RenderPassContext& ctx) override;

	/**
	 * @brief サブビューを描くコールバックを設定する
	 * @param callback 登録済みのサブビューを順に描く処理
	 */
	void SetCallback(std::function<void()> callback) { callback_ = std::move(callback); }

private:
	std::function<void()> callback_;
};

/**
 * @brief バックバッファを描画可能な状態にするパス
 *
 * @details 出力先がバックバッファのときだけ動く。
 *          エディタではシーンをレンダーターゲットへ描き出すため、
 *          バックバッファの用意はImGuiの描画直前にアプリ側が行う。
 */
class BackBufferPreparePass : public IRenderPass
{
public:
	const char* GetName() const override { return "BackBufferPrepare"; }
	void Execute(const RenderPassContext& ctx) override;

	/**
	 * @brief 出力先がバックバッファのときだけ実行する
	 */
	bool ShouldExecute(const RenderPassContext& ctx) const override
	{
		return IsEnabled() && ctx.outputTarget == nullptr;
	}
};

/**
 * @brief ポストプロセスパス
 * @details ブルームの合成とトーンマップを行い、HDRのシーンをLDRへ落とす。
 */
class PostProcessPass : public IRenderPass
{
public:
	const char* GetName() const override { return "PostProcess"; }
	void Execute(const RenderPassContext& ctx) override;
};

/**
 * @brief 2Dスプライト描画パス
 * @details ポストプロセスの後に描くことで、UIがブルームの影響を受けないようにする。
 */
class Sprite2DPass : public IRenderPass
{
public:
	const char* GetName() const override { return "Sprite2D"; }
	void Execute(const RenderPassContext& ctx) override;
};

/**
 * @brief 標準的な描画順でパイプラインを組み立てる
 *
 * @details 既存の描画順をそのまま並べたもの。
 *          シャドウ → G-Buffer → ライト → フォワード → Skybox →
 *          パーティクル → 解決 → ポストプロセス → 2D。
 *
 * @param pipeline 組み立て先のパイプライン。既存のパスは破棄される
 */
void BuildStandardRenderPipeline(RenderPipeline& pipeline);

/**
 * @brief サブビュー用にシーンだけを描くパイプラインを組み立てる
 *
 * @details 中継映像・カメラプレビュー・反射など、
 *          「シーンをレンダーターゲットに焼くだけ」のビュー向け。
 *
 *          シャドウマップは本編と共有するのでここには含めない。
 *          ポストプロセスと2Dも含まない。必要ならビューごとに足すこと。
 *
 * @param pipeline 組み立て先のパイプライン。既存のパスは破棄される
 */
void BuildSceneOnlyRenderPipeline(RenderPipeline& pipeline);
} // namespace KCE
