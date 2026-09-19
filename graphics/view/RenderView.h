#pragma once
#include <memory>
#include <string>

#include <dxgiformat.h>

#include "graphics/RenderFormats.h"
#include "graphics/view/RenderLayer.h"
#include "time/ClockId.h"

namespace KCE
{
class Camera;
class DirectXCommon;
class GBuffer;
class RenderTexture;
class SrvManager;

/**
 * @brief ビューごとに省けるパス
 * @details サブビュー（モニター・反射など）は本編ほどの見た目が要らないことが多い。
 *          描かなくてよいものを落として軽くするのに使う。
 */
enum class RenderViewPass : uint32_t
{
	Outline = 1u << 0, //!< 輪郭線
	Fog = 1u << 1,	   //!< 大気フォグ
	Beams = 1u << 2,   //!< スポットライトのビーム
	Text3D = 1u << 3,  //!< 3D 空間の文字
	DebugLines = 1u << 4, //!< デバッグ用の線（グリッド・ライトの形など）
};

/**
 * @brief 1つの視点ぶんの描画リソースをまとめたもの
 *
 * @details SEQUENCER_PLAN 4.3。従来は renderTexture_ / deferredRenderer_ /
 *          GBuffer がいずれも単一インスタンス固定で、
 *          「任意のカメラで任意のレンダーターゲットにシーンを描く」手段が無かった。
 *
 *          これが無いと以下が全て実現できない。
 *          - ステージモニターへの中継映像（サブカメラ → RT）
 *          - エディタのカメラプレビュー小窓
 *          - 床の平面反射
 *          - ミニマップ、キャラクタープレビュー
 *
 *          ビューは「カメラ」「G-Buffer」「出力先」「描画対象フィルタ」を持つ。
 *          G-Buffer は解像度ごとに必要なのでビューが所有するが、
 *          パイプラインステートは DeferredRenderer が共有する。
 *
 *          サブビューは本編と同じ解像度である必要はない。
 *          中継映像は解像度を落として作るのが前提。
 */
class RenderView
{
public:
	RenderView();
	~RenderView();

	/**
	 * @brief 初期化する
	 * @param dxCommon DirectXCommonへのポインタ
	 * @param srvManager SrvManagerへのポインタ
	 * @param name ビューの名前（エディタでの表示用）
	 * @param width 幅
	 * @param height 高さ
	 * @param colorFormat 出力するカラーのフォーマット
	 */
	void Initialize(
		DirectXCommon* dxCommon,
		SrvManager* srvManager,
		const std::string& name,
		uint32_t width,
		uint32_t height,
		DXGI_FORMAT colorFormat = kSceneColorFormat);

	/**
	 * @brief 解像度を変更する
	 * @details G-Bufferと出力先をまとめて作り直す。
	 *          GPUの完了を待ってから呼ぶこと。
	 * @param width 新しい幅
	 * @param height 新しい高さ
	 */
	void Resize(uint32_t width, uint32_t height);

	/**
	 * @brief このビューを描くカメラを設定する
	 * @details nullptr の場合、描画時はアクティブカメラが使われる。
	 * @param camera カメラ。所有権は持たない
	 */
	void SetCamera(Camera* camera) { camera_ = camera; }
	Camera* GetCamera() const { return camera_; }

	GBuffer* GetGBuffer() const { return gBuffer_.get(); }
	RenderTexture* GetSceneColor() const { return sceneColor_.get(); }

	/**
	 * @brief 選択的ブルーム用の発光バッファを取得
	 * @return 発光バッファ。所有しない
	 * @details 光らせたいものだけがここに書く。ポストプロセスはこれをぼかしてシーンに足す。
	 */
	RenderTexture* GetBloomMask() const { return bloomMask_.get(); }

	uint32_t GetWidth() const { return width_; }
	uint32_t GetHeight() const { return height_; }

	const std::string& GetName() const { return name_; }

	/**
	 * @brief 描画対象のレイヤーマスクを設定する
	 * @param mask 描きたいレイヤーの集合
	 */
	void SetLayerMask(RenderLayerMask mask) { layerMask_ = mask; }
	RenderLayerMask GetLayerMask() const { return layerMask_; }

	/**
	 * @brief このビューを描画するかどうか
	 * @details 中継映像を30fpsに間引くなど、フレーム単位で落とすのに使う。
	 * @param enabled 描画するなら真
	 */
	void SetEnabled(bool enabled) { enabled_ = enabled; }
	bool IsEnabled() const { return enabled_; }

	/** @brief 次に有効なフレームでは更新間隔を待たず描き直す */
	void RequestImmediateUpdate() { hasRendered_ = false; }

	/**
	 * @brief 何秒ごとに描き直すか
	 * @details テレビのように 24fps で十分なモニターなどで使う。描かないフレームは前の絵が残る。
	 * @param seconds 描き直す間隔（秒）。0 以下なら毎フレーム描く
	 */
	void SetUpdateInterval(float seconds) { updateInterval_ = seconds; }
	float GetUpdateInterval() const { return updateInterval_; }

	/**
	 * @brief 描き直す間隔を測る時計を決める
	 * @param clock 指定なしなら Editor（編集中でも描き直す）。ゲームの一時停止で止めたいなら Game を指す
	 */
	void SetClock(ClockId clock) { clock_ = clock; }
	ClockId GetClock() const { return clock_; }

	/**
	 * @brief 経過時間を進めて、このフレームで描き直すかを決める
	 * @details 一度も描いていなければ必ず描く（作りたての絵は読めない状態のため）。
	 *          大きく遅れたときに、取り戻そうとして連続で描かないよう余りは捨てる。
	 * @param deltaSeconds 前のフレームからの実時間（秒）
	 * @return 描き直すなら真
	 */
	bool AdvanceAndCheckUpdate(float deltaSeconds)
	{
		if (updateInterval_ <= 0.0f || !hasRendered_)
		{
			hasRendered_ = true;
			timeSinceUpdate_ = 0.0f;
			return true;
		}
		timeSinceUpdate_ += deltaSeconds;
		if (timeSinceUpdate_ < updateInterval_)
		{
			return false;
		}
		timeSinceUpdate_ -= updateInterval_;
		if (timeSinceUpdate_ >= updateInterval_)
		{
			timeSinceUpdate_ = 0.0f;
		}
		return true;
	}

	/**
	 * @brief このビューで特定のパスを描くかどうか
	 * @param pass 対象のパス
	 * @param enabled 描くなら真（既定はすべて描く）
	 */
	void SetPassEnabled(RenderViewPass pass, bool enabled)
	{
		const uint32_t bit = static_cast<uint32_t>(pass);
		disabledPasses_ = enabled ? (disabledPasses_ & ~bit) : (disabledPasses_ | bit);
	}
	bool IsPassEnabled(RenderViewPass pass) const { return (disabledPasses_ & static_cast<uint32_t>(pass)) == 0; }

	/**
	 * @brief 描画に使える状態か
	 * @return 初期化済みなら真
	 */
	bool IsValid() const { return gBuffer_ != nullptr && sceneColor_ != nullptr; }

private:
	// ビューの名前
	std::string name_;
	// このビューを描くカメラ。所有権は持たない
	Camera* camera_ = nullptr;
	// このビュー専用のG-Buffer
	std::unique_ptr<GBuffer> gBuffer_;
	// シーンの描画結果（HDR）
	std::unique_ptr<RenderTexture> sceneColor_;
	// 選択的ブルーム用。シーンカラーと同じ大きさ・形式で持つ
	std::unique_ptr<RenderTexture> bloomMask_;

	uint32_t width_ = 0;
	uint32_t height_ = 0;

	// 描画対象のレイヤー
	RenderLayerMask layerMask_ = kRenderLayerAll;
	// 描画するかどうか
	bool enabled_ = true;
	// 描き直す間隔（秒）。0 以下なら毎フレーム
	// 描き直す間隔を測る時計。指定なしなら Editor
	ClockId clock_{};
	float updateInterval_ = 0.0f;
	// 前に描いてからの経過（秒）
	float timeSinceUpdate_ = 0.0f;
	// 一度でも描いたか
	bool hasRendered_ = false;
	// 省くパス（RenderViewPass のビットの集まり）
	uint32_t disabledPasses_ = 0;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
};
} // namespace KCE
