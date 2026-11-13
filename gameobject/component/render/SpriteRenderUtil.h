#pragma once
#include <cmath>
#include <string>

#include "base/GraphicsTypes.h"
#include "manager/graphics/TextureManager.h"
#ifdef USE_IMGUI
#include <algorithm>

#include "imgui/imgui.h"
#endif

namespace KCE
{
namespace GameObjectComponent
{
/** @brief 新しく置いたときの画像。エンジンが必ず読み込んでいる 1x1 の白 */
inline constexpr const char* kDefaultSpriteTexture = "textures/white1x1.png";

/**
 * @brief 持ち主のワールド行列から、画面（1920x1080）での位置・倍率・回転を取り出す
 * @details Transform の XY が画面の px、Z 回転が画面内の回転、XY のスケールが大きさの倍率。親子もこの行列に入っている（行ベクトル）
 */
struct ScreenPlacement
{
	float x = 0.0f;
	float y = 0.0f;
	float scaleX = 1.0f;
	float scaleY = 1.0f;
	float rotation = 0.0f;
};

inline ScreenPlacement DecomposeScreenPlacement(const Matrix4x4& world)
{
	ScreenPlacement placement;
	placement.x = world.m[3][0];
	placement.y = world.m[3][1];
	placement.scaleX = std::sqrt(world.m[0][0] * world.m[0][0] + world.m[0][1] * world.m[0][1]);
	placement.scaleY = std::sqrt(world.m[1][0] * world.m[1][0] + world.m[1][1] * world.m[1][1]);
	placement.rotation = std::atan2(world.m[0][1], world.m[0][0]);
	return placement;
}

/** @brief 画像が読み込み済みか。未読み込みの名前で Sprite を作ったり替えたりしないための確認 */
inline bool IsTextureLoaded(const std::string& path)
{
	uint32_t index = 0;
	return !path.empty() && TextureManager::GetInstance()->TryGetTextureIndexByFilePath(path, index);
}

#ifdef USE_IMGUI
/**
 * @brief 読み込み済みの 2D 画像から選ぶプルダウン。カーソルを置くとプレビューが出る
 * @param allowNone 真なら「（なし）」を選べる（空文字になる）
 * @return 変わったら真
 */
inline bool DrawTextureCombo(const char* label, std::string& path, bool allowNone = false)
{
	bool changed = false;
	const auto textures = TextureManager::GetInstance()->GetLoadedTextures();
	const char* preview = path.empty() ? "（なし）" : path.c_str();
	if (ImGui::BeginCombo(label, preview))
	{
		if (allowNone && ImGui::Selectable("（なし）", path.empty()))
		{
			path.clear();
			changed = true;
		}
		for (const auto& texture : textures)
		{
			if (!texture.previewable) { continue; }
			if (ImGui::Selectable(texture.path.c_str(), texture.path == path))
			{
				path = texture.path;
				changed = true;
			}
			if (ImGui::IsItemHovered() && texture.gpuHandle != 0 && texture.width > 0 && texture.height > 0)
			{
				const float scale = (std::min)(192.0f / static_cast<float>(texture.width), 192.0f / static_cast<float>(texture.height));
				ImGui::BeginTooltip();
				ImGui::Image(static_cast<ImTextureID>(texture.gpuHandle), ImVec2(static_cast<float>(texture.width) * scale, static_cast<float>(texture.height) * scale));
				ImGui::EndTooltip();
			}
		}
		ImGui::EndCombo();
	}
	return changed;
}
#endif
} // namespace GameObjectComponent
} // namespace KCE
