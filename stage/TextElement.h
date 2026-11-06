#pragma once

#include <memory>
#include <string>

#include "graphics/text/TextMesh3D.h"
#include "stage/IStageElement.h"

namespace KCE
{
class Text3DRenderer;

/**
 * @brief ステージに置く 3D 文字 1 つ。TextMesh3D を所有し、Text3DRenderer へ非所有で登録する。
 *
 * JSON は旧ステージの "texts3d" と同じ形:
 *   { "name": "title", "text": "...", "position": [x,y,z], "rotation": [x,y,z,w],
 *     "scale": 1, "size": 1, "color": [r,g,b,a], "style": "Fade" }
 *
 * - 一覧と Inspector は Text3DRenderer が名前で持っているので、ここでは選択を渡すだけ
 * - reveal / exit はシーケンスが動かす値なので、保存も復元もしない（読み込み時は全部表示）
 */
class TextElement final : public IStageElement
{
public:
	TextElement(std::string name, std::unique_ptr<TextMesh3D> mesh, Text3DRenderer* renderer);
	~TextElement() override;

	/** @brief StageManager に登録する、"texts3d" の種類 */
	static StageElementType MakeType();

	const char* GetTypeKey() const override { return "texts3d"; }
	nlohmann::ordered_json Serialize() const override;
	bool OnRename(const std::string& newName) override;

	/** @return ゲームから文字を変えたいときの窓口。所有は TextElement */
	TextMesh3D* GetMesh() const { return mesh_.get(); }

#ifdef USE_IMGUI
	void DrawHierarchyRow() override;
	void DrawInspector(const SelectionItem& selected) override;
	bool GetGizmoPose(const SelectionItem& selected, Matrix4x4& world, uint32_t& operations) const override;
	void ApplyGizmo(const SelectionItem& selected, const GizmoResult& result, uint32_t dragId) override;
#endif

private:
	std::unique_ptr<TextMesh3D> mesh_;
	// Framework 所有。TextElement より長生きする
	Text3DRenderer* renderer_ = nullptr;
};
} // namespace KCE
