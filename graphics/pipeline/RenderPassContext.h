#pragma once

namespace KCE
{
class CameraManager;
class DeferredRenderer;
class BeamRenderer;
class DirectXCommon;
class FogRenderer;
class FrameConstantAllocator;
class LightManager;
class Object3dCommon;
class OutlineRenderer;
class PostProcessManager;
class RenderTexture;
class RenderView;
class SceneManager;
class ShadowMapManager;
class ShadowMapPipeline;
class Skybox;
class SpriteCommon;
class SrvManager;
class TextOverlay;
class Text3DRenderer;
class FxaaRenderer;
class DepthOfFieldRenderer;
class VolumetricLightRenderer;
class PlanarReflection;

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

	// --- ビュー ---
	/**
	 * @brief 今描いている視点
	 *
	 * @details カメラ・G-Buffer・出力先・描画対象フィルタを束ねたもの。
	 *          パスはここからカメラとレンダーターゲットを取る。
	 *          同じパイプラインを別のビューに対して走らせることで、
	 *          中継映像やカメラプレビューを作れる。
	 */
	RenderView* view = nullptr;

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

	// --- 定数 ---
	/**
	 * @brief フレーム単位の定数割り当て器
	 * @details ビューごとに値が変わる定数（WVP など）は、ここから描画のたびに
	 *          新しい領域を切り出して書く。定数バッファを1つだけ持つと、
	 *          複数ビューで描いたとき最後に書いた値で全ビューが描かれる。
	 */
	FrameConstantAllocator* frameConstantAllocator = nullptr;

	// --- 大気 ---
	//! 大気フォグ
	FogRenderer* fogRenderer = nullptr;
	//! スポットライトのビーム
	BeamRenderer* beamRenderer = nullptr;

	// --- NPR ---
	//! アウトライン（輪郭線）
	OutlineRenderer* outlineRenderer = nullptr;

	// --- 文字 ---
	//! 歌詞テロップと会話枠（2D の一番手前）
	TextOverlay* textOverlay = nullptr;
	//! 3D 空間の文字（半透明の後）
	Text3DRenderer* text3DRenderer = nullptr;

	// --- 画の質 ---
	//! トーンマップ後のジャギーを均す
	FxaaRenderer* fxaaRenderer = nullptr;
	//! 被写界深度（ポストプロセスの前）
	DepthOfFieldRenderer* depthOfFieldRenderer = nullptr;
	//! スポットライトのボリュメトリック
	VolumetricLightRenderer* volumetricLightRenderer = nullptr;
	//! 床の平面反射
	PlanarReflection* planarReflection = nullptr;

	/**
	 * @brief 最低限の要素が揃っているか
	 * @return 揃っていれば真
	 */
	bool IsValid() const
	{
		return dxCommon != nullptr && srvManager != nullptr && sceneManager != nullptr && view != nullptr;
	}
};
} // namespace KCE
