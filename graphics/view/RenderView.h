#pragma once
#include <memory>
#include <string>

#include <dxgiformat.h>

#include "graphics/RenderFormats.h"
#include "graphics/view/RenderLayer.h"

namespace KCE
{
class Camera;
class DirectXCommon;
class GBuffer;
class RenderTexture;
class SrvManager;

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

	uint32_t width_ = 0;
	uint32_t height_ = 0;

	// 描画対象のレイヤー
	RenderLayerMask layerMask_ = kRenderLayerAll;
	// 描画するかどうか
	bool enabled_ = true;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
};
} // namespace KCE
