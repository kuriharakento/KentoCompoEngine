#pragma once

namespace KCE
{
class CameraManager;
class DeferredRenderer;
class DirectXCommon;
class LightManager;
class Object3dCommon;
class PostProcessManager;
class RenderTexture;
class SceneManager;
class ShadowMapManager;
class ShadowMapPipeline;
class Skybox;
class SpriteCommon;
class SrvManager;

/**
 * @brief 描画パスが1フレームの実行に必要とするものをまとめた入れ物
 *
 * @details SEQUENCER_PLAN 4.2。従来は描画順がアプリ側に約200行の直列コードで
 *          書かれており、ルートパラメータ番号まで手書きされていた。
 *          パスを足すたびにその中を書き換える必要があり、
 *          `#ifdef USE_IMGUI` で最終出力先が分岐しているため両経路を直す必要もあった。
 *
 *          各パスはこの構造体から必要なものだけを取り出す。
 *          ポインタの所有権は持たず、寿命は Framework が保証する。
 *
 *          自動バリア解決やリソースエイリアシングまで備えた
 *          フルスペックのRenderGraphは過剰であり、
 *          パスの並びと入出力RTの受け渡しが整理されていれば十分である。
 */
struct RenderPassContext
{
	// --- システム ---
	DirectXCommon* dxCommon = nullptr;
	SrvManager* srvManager = nullptr;

	// --- シーンの構成要素 ---
	CameraManager* cameraManager = nullptr;
	LightManager* lightManager = nullptr;
	SceneManager* sceneManager = nullptr;
	Skybox* skybox = nullptr;

	// --- 描画の共通設定 ---
	Object3dCommon* objectCommon = nullptr;
	SpriteCommon* spriteCommon = nullptr;

	// --- シャドウ ---
	ShadowMapManager* shadowMapManager = nullptr;
	ShadowMapPipeline* shadowMapPipeline = nullptr;
	//! シャドウマップのニアクリップ距離
	float shadowNearPlane = 0.1f;
	//! シャドウマップのファークリップ距離
	float shadowFarPlane = 200.0f;

	// --- ディファード ---
	DeferredRenderer* deferredRenderer = nullptr;

	// --- レンダーターゲット ---
	/**
	 * @brief シーンを描くHDRのレンダーターゲット
	 * @details ライトパスからパーティクルまでがここへ描き込む。
	 */
	RenderTexture* sceneColor = nullptr;

	/**
	 * @brief ポストプロセスの出力先
	 *
	 * @details nullptr の場合はバックバッファへ直接出力する。
	 *          エディタではシーンをImGuiのウィンドウに表示するため、
	 *          ここにレンダーターゲットが入る。
	 *          `#ifdef` で経路を分けるのではなく、この値が
	 *          nullptr かどうかで各パスが振る舞いを変える。
	 */
	RenderTexture* outputTarget = nullptr;

	// --- ポストプロセス ---
	PostProcessManager* postProcessManager = nullptr;

	/**
	 * @brief 最低限の要素が揃っているか
	 * @return 揃っていれば真
	 */
	bool IsValid() const
	{
		return dxCommon != nullptr && srvManager != nullptr && sceneManager != nullptr && sceneColor != nullptr;
	}
};
} // namespace KCE
