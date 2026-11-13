#pragma once
#include <memory>
#include <string>

#include "engine/gameobject/component/base/Component.h"
#include "engine/gameobject/component/base/ISpriteRenderableComponent.h"
#include "engine/gameobject/component/render/SpriteRenderUtil.h"
#include "graphics/2d/Sprite.h"
#include "jsonEditor/JsonEditableBase.h"

namespace KCE
{
namespace GameObjectComponent
{
/**
 * @brief 静的ライブラリから、この .obj を確実に取り込ませるための空関数（REGISTER_COMPONENT を走らせる）
 */
void ForceLinkSpriteRendererComponent();

/**
 * @brief 画面（1920x1080）に 2D の画像を1枚描くコンポーネント
 *
 * - 位置は持ち主の Transform の XY（画面の px）、画面内の回転は Z 回転、大きさは「サイズ(px)」に XY のスケールを掛けた値
 * - 描く順は order（小さいほど奥）。同じなら木をたどった順
 * - 描画のオンオフは GameObject::SetVisible、更新ごと止めるなら SetActive
 * - 3D の形を持たないので、持ち主の本体の立方体は映さない
 * - 保存はプレハブ（GameObject::SavePrefab）。画像は読み込み済みの名前で持ち、未読み込みなら今のまま
 */
class SpriteRendererComponent : public Component, public ISpriteRenderableComponent, public JsonEditableBase
{
public:
	SpriteRendererComponent();

	/** @brief 持ち主の本体の立方体を映さないようにする */
	void Awake() override;

	int GetSpriteOrder() const override { return order_; }
	void DrawSprite2D() override;

	/** @brief 画像を設定する。読み込み済みの名前だけ反映される */
	void SetTexture(const std::string& texturePath) { texture_ = texturePath; }
	const std::string& GetTexture() const { return texture_; }
	/** @brief 色（RGBA）を設定する */
	void SetColor(const Vector4& color) { color_ = color; }
	const Vector4& GetColor() const { return color_; }
	/** @brief サイズ(px)。Transform のスケールが掛かる */
	void SetSize(float width, float height) { width_ = width; height_ = height; }
	/** @brief 描く順。小さいほど奥 */
	void SetOrder(int order) { order_ = order; }

	/** @return 中の Sprite。まだ作られていなければ nullptr。所有権は移らない */
	Sprite* GetSprite() const { return sprite_.get(); }

#ifdef USE_IMGUI
	void DrawImGui() override;
#endif

private:
	/** @brief Sprite が無ければ作る。スプライト共通部がまだ無ければ偽 */
	bool EnsureSprite();
	/** @brief 画像など、変えられた設定を Sprite へ反映する */
	void ApplySettings();

	// 所有する描画物。DrawSprite2D で初めて作る（スプライト共通部が揃うのを待つため）
	std::unique_ptr<Sprite> sprite_;

	// --- プレハブに保存する値 ---
	std::string texture_ = kDefaultSpriteTexture;
	// 0 以下なら、最初に作ったときの画像の大きさにする
	float width_ = 0.0f;
	float height_ = 0.0f;
	float anchorX_ = 0.5f;
	float anchorY_ = 0.5f;
	Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
	bool flipX_ = false;
	bool flipY_ = false;
	int order_ = 0;

	// Sprite へ反映済みの画像
	std::string appliedTexture_;
};
} // namespace GameObjectComponent
} // namespace KCE
