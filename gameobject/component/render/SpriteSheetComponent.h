#pragma once
#include <algorithm>
#include <memory>
#include <string>

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
void ForceLinkSpriteSheetComponent();

/**
 * @brief スプライトシート（コマを格子に並べた1枚の画像）から、指定の1コマだけを板に映すコンポーネント
 *
 * - 画像を columns x rows の格子に割り、frame 番号（0 始まり、左上から右へ、行が進んだら次の行）のコマを映す
 * - 画面（既定は 3D の world3d = 真）: GaugeComponent と同じ置き方ができる
 *   - 画面: Transform の XY が画面の px、order の小さい順に描く
 *   - ワールド: Transform の位置に板を置く。billboard ならカメラへ向ける。大きさは「サイズ(px)」を pixelsPerUnit で割った長さ
 * - 数字や、アイコンの切り替えに使う。複数桁は、子の板を並べ、NumberDisplay で値を割り振る
 * - 描画のオンオフは GameObject::SetVisible
 */
class SpriteSheetComponent : public Behaviour, public ISpriteRenderableComponent, public JsonEditableBase
{
public:
	SpriteSheetComponent();

	/** @brief 持ち主の本体の立方体を映さないようにする */
	void Awake() override;

	int GetSpriteOrder() const override { return order_; }
	bool IsScreenSpace() const override { return !world3d_; }
	/** @brief 画面モードのとき、1コマを描く */
	void DrawSprite2D() override;
	/** @brief ワールドモードのとき、1コマを描く（半透明の段階） */
	void DrawTransparent3D(CameraManager* camera) override;

	/** @brief 映すコマの番号。範囲外は、描くときに丸める */
	void SetFrame(int frame) { frame_ = frame; }
	int GetFrame() const { return frame_; }
	/** @brief コマの数（columns x rows） */
	int GetFrameCount() const { return (std::max)(columns_, 1) * (std::max)(rows_, 1); }

	void SetTint(const Vector4& tint) { tint_ = tint; }
	const Vector4& GetTint() const { return tint_; }
	/** @brief 画像と格子を設定する。読み込み済みの名前だけ反映される */
	void SetSheet(const std::string& texture, int columns, int rows);

	void SetWorld3d(bool world3d) { world3d_ = world3d; }
	bool IsWorld3d() const { return world3d_; }

#ifdef USE_IMGUI
	void DrawImGui() override;
#endif

private:
	/** @brief Sprite を用意し、画像と、映すコマを反映する。スプライト共通部がまだ無ければ偽 */
	bool Prepare();

	std::unique_ptr<Sprite> sprite_;

	// --- プレハブに保存する値 ---
	std::string texture_ = "./Resources/textures/ui/numbers.png";
	int columns_ = 10;
	int rows_ = 1;
	int frame_ = 0;
	// 画面では px、ワールドでは px / pixelsPerUnit
	float width_ = 64.0f;
	float height_ = 64.0f;
	float anchorX_ = 0.5f;
	float anchorY_ = 0.5f;
	Vector4 tint_ = { 1.0f, 1.0f, 1.0f, 1.0f };
	int order_ = 0;
	// 真なら 3D のワールドに置く
	bool world3d_ = true;
	// ワールド: カメラへ向ける
	bool billboard_ = true;
	// ワールド: 奥の物に隠れる
	bool depthTest_ = true;
	// ワールド: サイズ(px)をワールドの長さにする割り算の値
	float pixelsPerUnit_ = 100.0f;

	// Sprite へ反映済みの画像
	std::string appliedTexture_;
	// 画像全体の大きさ（px）。コマの大きさを割り出すのに使う
	Vector2 sheetSize_ = { 0.0f, 0.0f };
};
} // namespace GameObjectComponent
} // namespace KCE
