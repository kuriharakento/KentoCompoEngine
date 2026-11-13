#pragma once
#include <memory>
#include <string>
#include <vector>

#include "engine/gameobject/component/base/Behaviour.h"
#include "engine/gameobject/component/base/ISpriteRenderableComponent.h"
#include "graphics/2d/Sprite.h"
#include "jsonEditor/JsonEditableBase.h"

namespace KCE
{
namespace GameObjectComponent
{
/**
 * @brief 静的ライブラリから、この .obj を確実に取り込ませるための空関数（REGISTER_COMPONENT を走らせる）
 */
void ForceLinkGaugeComponent();

/**
 * @brief 円形（ラジアルワイプ）のゲージ。画面の 2D にも、3D のワールドにも置ける
 *
 * - 前景の画像を、中心から扇形に切り抜いて割合ぶんだけ表示する。背景の画像は常に全体を描く（無くてもよい）
 * - 割合 ratio（0〜1）は、ゲームから SetRatio で渡す。見た目は followSpeed でなめらかに追いかける
 * - 色は「割合 → 色」の段階（colorStops）で決める。段階の間は補間する。無ければ tint の色のまま
 * - 画面（既定）: Transform の XY が画面の px、Z 回転が画面内の回転、order の小さい順に描く（GameObjectManager::DrawSprites2D）
 * - ワールド（world3d）: Transform の位置に板を置く。billboard ならカメラへ向け、そうでなければ Transform の向きのまま。
 *   大きさは「サイズ(px)」を pixelsPerUnit で割ったワールドの大きさ。3D のシーンの半透明として、本編のフォワードのあとに描く
 * - 描画のオンオフは GameObject::SetVisible、更新ごと止めるなら SetActive
 */
class GaugeComponent : public Behaviour, public ISpriteRenderableComponent, public JsonEditableBase
{
public:
	GaugeComponent();

	/** @brief 持ち主の本体の立方体を映さないようにする */
	void Awake() override;
	/** @brief 見た目の割合を、目標の割合へ近づける */
	void Update() override;

	int GetSpriteOrder() const override { return order_; }
	bool IsScreenSpace() const override { return !world3d_; }
	/** @brief 画面モードのとき、背景と前景を描く */
	void DrawSprite2D() override;
	/** @brief ワールドモードのとき、背景と前景を描く（本編のフォワードのあと） */
	void Draw3D(CameraManager* camera) override;

	/** @brief 目標の割合（0〜1）。範囲外は丸める */
	void SetRatio(float ratio);
	float GetRatio() const { return ratio_; }
	/** @brief 追いかけずに、見た目もすぐこの割合にする */
	void SetRatioImmediate(float ratio);
	/** @brief いま見えている割合（追従の途中の値） */
	float GetDisplayRatio() const { return displayRatio_; }

	/** @brief 画像を設定する。前景は必須、背景は空で無し。読み込み済みの名前だけ反映される */
	void SetTextures(const std::string& foreTexture, const std::string& backTexture);
	/** @brief 前景の色の基準（RGBA）。色の段階の色に掛ける */
	void SetTint(const Vector4& tint) { tint_ = tint; }
	/** @brief 背景の色（RGBA）。前景とは別に決める */
	void SetBackTint(const Vector4& tint) { backTint_ = tint; }
	/** @brief 割合に応じた色の段階を設定する。ratio の小さい順でなくてもよい */
	struct ColorStop
	{
		float ratio = 0.0f;
		Vector3 color = { 1.0f, 1.0f, 1.0f };
	};
	void SetColorStops(const std::vector<ColorStop>& stops);
	/** @brief 割合に対応する前景の色（tint を掛けた後） */
	Vector4 EvaluateColor(float ratio) const;

	/** @brief 画面に置くか、3D のワールドに置くか */
	void SetWorld3d(bool world3d) { world3d_ = world3d; }
	bool IsWorld3d() const { return world3d_; }

#ifdef USE_IMGUI
	void DrawImGui() override;
#endif

private:
	/** @brief 前景（と、あれば背景）の Sprite を用意し、画像などの設定を反映する。スプライト共通部がまだ無ければ偽 */
	bool Prepare();
	/** @brief 見た目の割合を、更新が止まっている間や最初の1回は目標へ合わせる */
	void SyncDisplayRatio();

	std::unique_ptr<Sprite> foreSprite_;
	std::unique_ptr<Sprite> backSprite_;

	// --- プレハブに保存する値 ---
	std::string foreTexture_ = kDefaultGaugeTexture;
	std::string backTexture_;
	// 画面では px、ワールドでは px / pixelsPerUnit
	float width_ = 256.0f;
	float height_ = 256.0f;
	float anchorX_ = 0.5f;
	float anchorY_ = 0.5f;
	// 目標の割合
	float ratio_ = 1.0f;
	// 追いかける速さ（1 秒あたり）。0 以下ならすぐ合わせる
	float followSpeed_ = 8.0f;
	// 開始角度（度）。真上が 0 で、時計回りに増える
	float startAngleDeg_ = 0.0f;
	bool clockwise_ = true;
	Vector4 tint_ = { 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4 backTint_ = { 1.0f, 1.0f, 1.0f, 1.0f };
	// 割合 → 色の段階。x,y,z が色、w が割合
	std::vector<Vector4> colorStops_;
	int order_ = 0;
	// 真なら 3D のワールドに置く
	bool world3d_ = false;
	// ワールド: カメラへ向ける
	bool billboard_ = true;
	// ワールド: 奥の物に隠れる
	bool depthTest_ = true;
	// ワールド: サイズ(px)をワールドの長さにする割り算の値
	float pixelsPerUnit_ = 100.0f;

	// 見た目の割合
	float displayRatio_ = 1.0f;
	bool displayInitialized_ = false;
	// Sprite へ反映済みの画像
	std::string appliedForeTexture_;
	std::string appliedBackTexture_;

	static constexpr const char* kDefaultGaugeTexture = "textures/white1x1.png";
};
} // namespace GameObjectComponent
} // namespace KCE
