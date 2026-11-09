#pragma once

#include <memory>
#include <string>

#include "graphics/2d/Sprite.h"
#include "level/ILevelElement.h"

namespace KCE
{
class SpriteCommon;
class SpriteLayer;

/**
 * @brief Level に置く 2D スプライト 1 つ。Sprite を所有し、SpriteLayer へ非所有で登録する。
 *
 * JSON の例（"sprites" 配列の1件）:
 *   { "name": "logo", "texture": "textures/logo.png",
 *     "position": [960, 540], "size": [512, 256], "rotation": 0.0,
 *     "anchor": [0.5, 0.5], "color": [1, 1, 1, 1], "flipX": false, "flipY": false, "order": 0 }
 *
 * - 座標は 1920x1080 の画面。order が小さいほど奥に描く
 * - シーケンサの「スプライト」トラックが、この名前で位置・大きさ・回転・色を動かす
 */
class SpriteElement final : public ILevelElement
{
public:
	SpriteElement(std::string name, std::string texturePath, std::unique_ptr<Sprite> sprite, SpriteLayer* layer, int order);
	~SpriteElement() override;

	/** @brief LevelManager に登録する、"sprites" の種類 */
	static LevelElementType MakeType();

	const char* GetTypeKey() const override { return "sprites"; }
	nlohmann::ordered_json Serialize() const override;
	bool OnRename(const std::string& newName) override;

	/** @return ゲームから動かしたいときの窓口。所有は SpriteElement */
	Sprite* GetSprite() const { return sprite_.get(); }

#ifdef USE_IMGUI
	void DrawHierarchyRow() override;
	void DrawInspector(const SelectionItem& selected) override;
#endif

private:
	std::string texturePath_;
	std::unique_ptr<Sprite> sprite_;
	// Framework 所有。SpriteElement より長生きする
	SpriteLayer* layer_ = nullptr;
	int order_ = 0;
};
} // namespace KCE
