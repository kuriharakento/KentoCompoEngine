#include "SpriteAnimationEditor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

#include "base/PathManager.h"
#include "graphics/2d/Sprite.h"
#include "manager/editor/DebugUIManager.h"
#include "manager/graphics/ResourceLifetime.h"
#include "manager/graphics/TextureManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace KCE
{
#ifdef USE_IMGUI
namespace
{
constexpr const char* kWindowName = "スプライトアニメーション###Sprite Animation Editor";
constexpr const char* kDefaultName = "NewSpriteAnimation";
constexpr const char* kTextureDirectory = "textures";
constexpr const char* kSaveDirectory = "spriteAnimations";
constexpr const char* kTexturePickerPopup = "##SpriteAnimTexturePicker";
constexpr const char* kKeyTexturePopup = "##SpriteAnimKeyTexturePicker";

// タイムラインの寸法（ピクセル）
constexpr float kRulerHeight = 24.0f;
constexpr float kLaneHeight = 72.0f;
constexpr float kThumbPadding = 4.0f;
constexpr float kMarkerRadius = 6.0f;
constexpr float kPlayheadTriangle = 6.0f;
constexpr float kTickHeightMajor = 12.0f;
constexpr float kTickHeightMinor = 6.0f;
constexpr float kPreviewSize = 128.0f;
constexpr float kInspectorWidth = 280.0f;
constexpr float kInspectorExtraHeight = 32.0f;
constexpr float kDurationInputWidth = 90.0f;
constexpr float kPanelPadding = 16.0f;
constexpr float kToolbarItemWidth = 100.0f;
constexpr float kNameInputWidth = 200.0f;
constexpr float kPreviewPositionWidth = 160.0f;
constexpr float kTickLabelOffset = 3.0f;
constexpr float kPlayheadTriangleScale = 1.5f;
constexpr float kMarkerGap = 2.0f;
constexpr float kHoldInset = 1.0f;
constexpr float kEdgeGrabWidth = 8.0f;
constexpr float kEdgeGrabRatio = 0.4f;
constexpr float kEdgeGrabOutside = 3.0f;
constexpr float kEdgeHandleHalfWidth = 2.0f;
constexpr float kBorderThickness = 1.0f;
constexpr float kSelectedBorderThickness = 2.0f;

// 時間まわり
constexpr float kTailSeconds = 1.0f;
constexpr float kSnapSeconds = 0.01f;
constexpr float kMajorTickSeconds = 1.0f;
constexpr float kMinorTickSecondsFine = 0.1f;
constexpr float kMinorTickSecondsCoarse = 0.5f;
// これ以上拡大していたら細かい目盛りを出す
constexpr float kFineTickMinPixelsPerSecond = 200.0f;
constexpr float kMinPixelsPerSecond = 40.0f;
constexpr float kMaxPixelsPerSecond = 600.0f;
constexpr float kMinSpeed = 0.1f;
constexpr float kMaxSpeed = 4.0f;
constexpr float kDragTimeSpeed = 0.01f;
constexpr float kMaxKeyDuration = 60.0f;
constexpr float kRoundHalf = 0.5f;
constexpr float kPaddingSides = 2.0f;
constexpr float kPreviewScaleMin = 0.05f;
constexpr float kPreviewScaleMax = 10.0f;
constexpr float kPreviewScaleDragSpeed = 0.01f;
constexpr float kPreviewPositionDragSpeed = 1.0f;
constexpr float kPreviewAnchor = 0.5f;

constexpr ImU32 kColorBackground = IM_COL32(32, 32, 36, 255);
constexpr ImU32 kColorRuler = IM_COL32(48, 48, 54, 255);
constexpr ImU32 kColorTick = IM_COL32(150, 150, 160, 255);
constexpr ImU32 kColorTickText = IM_COL32(190, 190, 200, 255);
constexpr ImU32 kColorLaneHold = IM_COL32(70, 90, 130, 70);
constexpr ImU32 kColorLaneHoldSelected = IM_COL32(100, 140, 220, 110);
constexpr ImU32 kColorKeyBorder = IM_COL32(120, 120, 130, 255);
constexpr ImU32 kColorKeySelected = IM_COL32(255, 200, 60, 255);
constexpr ImU32 kColorMarker = IM_COL32(190, 190, 200, 255);
constexpr ImU32 kColorMissing = IM_COL32(160, 60, 60, 255);
constexpr ImU32 kColorEdgeHandle = IM_COL32(255, 255, 255, 220);
constexpr ImU32 kColorPlayhead = IM_COL32(255, 255, 255, 255);

/** @brief ImGui に渡す矩形の幅と高さを縦横比を保って枠に収める */
ImVec2 FitInto(float srcWidth, float srcHeight, float boxSize)
{
	if (srcWidth <= 0.0f || srcHeight <= 0.0f)
	{
		return ImVec2(boxSize, boxSize);
	}
	const float scale = boxSize / (std::max)(srcWidth, srcHeight);
	return ImVec2(srcWidth * scale, srcHeight * scale);
}
} // namespace
#endif

SpriteAnimationEditor::SpriteAnimationEditor() = default;

SpriteAnimationEditor::~SpriteAnimationEditor()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->Unregister(this);
	}
#endif
}

void SpriteAnimationEditor::Initialize(SpriteCommon* spriteCommon)
{
	spriteCommon_ = spriteCommon;
#ifdef USE_IMGUI
	std::snprintf(nameBuffer_.data(), nameBuffer_.size(), "%s", kDefaultName);
	DebugUIManager::GetInstance()->RegisterWindow(this, kWindowName, [this]() { this->DrawImGui(); }, EditorDock::Bottom, false);
#endif
}

void SpriteAnimationEditor::DrawScenePreview()
{
#ifdef USE_IMGUI
	if (!showOnScreen_ || !spriteCommon_)
	{
		return;
	}

	const int keyIndex = FindDisplayedKey();
	if (keyIndex < 0)
	{
		return;
	}
	// 読み込み済みの画像だけ出す（未読み込みだと代わりの画像が出てしまう）
	const std::string& path = clip_.GetKeys()[static_cast<size_t>(keyIndex)].texturePath;
	uint32_t srvIndex = 0;
	if (!TextureManager::GetInstance()->TryGetTextureIndexByFilePath(path, srvIndex))
	{
		return;
	}

	if (!previewSprite_)
	{
		previewSprite_ = std::make_unique<Sprite>();
		previewSprite_->Initialize(spriteCommon_, path);
		previewTexturePath_ = path;
	}
	else if (previewTexturePath_ != path)
	{
		previewSprite_->SetTexture(path);
		previewTexturePath_ = path;
	}

	// SetTexture で大きさが画像に戻るので、毎フレーム拡大率から決め直す
	const KCE::Vector2& textureSize = previewSprite_->GetTextureSize();
	previewSprite_->SetAnchorPoint({ kPreviewAnchor, kPreviewAnchor });
	previewSprite_->SetPosition({ previewPosition_[0], previewPosition_[1] });
	previewSprite_->SetSize({ textureSize.x * previewScale_, textureSize.y * previewScale_ });
	previewSprite_->Update();
	previewSprite_->Draw();
#endif
}

#ifdef USE_IMGUI
void SpriteAnimationEditor::DrawImGui()
{
	AdvancePlayback();

	// 選んだキーを Delete で消す
	if (selectedKey_ >= 0 && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
		&& !ImGui::IsAnyItemActive() && ImGui::IsKeyPressed(ImGuiKey_Delete))
	{
		clip_.RemoveKey(static_cast<size_t>(selectedKey_));
		selectedKey_ = -1;
	}

	DrawToolbar();
	ImGui::Separator();

	// 低いドックでも見えるよう、タイムラインを先に出す
	DrawTimeline();

	DrawPreview();
	ImGui::SameLine();
	DrawKeyInspector();

	if (!statusMessage_.empty())
	{
		ImGui::TextDisabled("%s", statusMessage_.c_str());
	}
}

void SpriteAnimationEditor::AdvancePlayback()
{
	if (!isPlaying_)
	{
		return;
	}
	const float duration = clip_.GetDuration();
	if (duration <= 0.0f)
	{
		time_ = 0.0f;
		isPlaying_ = false;
		return;
	}

	time_ += ImGui::GetIO().DeltaTime * speed_;
	if (!clip_.IsLooping() && time_ >= duration)
	{
		// 非ループは最後の画像で止める
		time_ = duration;
		isPlaying_ = false;
		return;
	}
	time_ = clip_.WrapTime(time_);
}

void SpriteAnimationEditor::DrawToolbar()
{
	const float duration = clip_.GetDuration();

	if (ImGui::Button(isPlaying_ ? "一時停止" : "再生"))
	{
		if (isPlaying_)
		{
			isPlaying_ = false;
		}
		else if (duration > 0.0f)
		{
			// 非ループで末尾に居たら頭から
			if (!clip_.IsLooping() && time_ >= duration)
			{
				time_ = 0.0f;
			}
			isPlaying_ = true;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("停止"))
	{
		isPlaying_ = false;
		time_ = 0.0f;
	}
	ImGui::SameLine();
	bool isLooping = clip_.IsLooping();
	if (ImGui::Checkbox("ループ", &isLooping))
	{
		clip_.SetLooping(isLooping);
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(kToolbarItemWidth);
	ImGui::SliderFloat("速度", &speed_, kMinSpeed, kMaxSpeed, "%.1fx");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(kToolbarItemWidth);
	ImGui::SliderFloat("拡大", &pixelsPerSecond_, kMinPixelsPerSecond, kMaxPixelsPerSecond, "%.0f px/s");
	ImGui::SameLine();
	ImGui::Text("現在 %.2f s / 総秒数 %.2f s / キー %d", time_, duration, static_cast<int>(clip_.GetKeys().size()));

	// 画像を選んで、現在位置にキーを足す
	if (ImGui::Button("画像を選ぶ"))
	{
		RefreshTextureList();
		ImGui::OpenPopup(kTexturePickerPopup);
	}
	DrawTexturePicker(kTexturePickerPopup, pendingTexture_);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(kDurationInputWidth);
	ImGui::DragFloat("表示時間 (s)", &newKeyDuration_, kDragTimeSpeed, kSpriteAnimationMinKeyDuration, kMaxKeyDuration, "%.2f");
	newKeyDuration_ = (std::max)(newKeyDuration_, kSpriteAnimationMinKeyDuration);
	ImGui::SameLine();
	if (ImGui::Button("末尾にキーを追加"))
	{
		// 今のアニメーションの終わりに続けて足す。画像を選んで連打すれば順に並ぶ
		AddKeyAt(clip_.GetDuration());
	}
	ImGui::SameLine();
	if (ImGui::Button("現在位置にキーを追加"))
	{
		AddKeyAt(time_);
	}
	// 選んだ画像の名前は右端に出す（長さでボタンの位置がずれないように）
	ImGui::SameLine();
	ImGui::Text("選択中: %s", pendingTexture_.empty() ? "(未選択)" : pendingTexture_.c_str());
	ImGui::SameLine();
	ImGui::TextDisabled("ダブルクリックでも追加");

	// 保存・読み込み
	ImGui::SetNextItemWidth(kNameInputWidth);
	ImGui::InputText("##SpriteAnimName", nameBuffer_.data(), nameBuffer_.size());
	ImGui::SameLine();
	if (ImGui::Button("保存"))
	{
		Save();
	}
	ImGui::SameLine();
	if (ImGui::Button("読み込み"))
	{
		Load();
	}
	ImGui::SameLine();
	if (ImGui::Button("クリア"))
	{
		clip_.Clear();
		selectedKey_ = -1;
		time_ = 0.0f;
		isPlaying_ = false;
	}

	// ゲーム画面（Scene）に実際のスプライトとして出す
	ImGui::SameLine();
	ImGui::Checkbox("ゲーム画面に表示", &showOnScreen_);
	if (showOnScreen_)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(kPreviewPositionWidth);
		ImGui::DragFloat2("位置", previewPosition_.data(), kPreviewPositionDragSpeed);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(kToolbarItemWidth);
		ImGui::DragFloat("倍率", &previewScale_, kPreviewScaleDragSpeed, kPreviewScaleMin, kPreviewScaleMax, "%.2f");
	}
}

void SpriteAnimationEditor::DrawPreview()
{
	ImGui::BeginChild("##SpriteAnimPreview", ImVec2(kPreviewSize + kPanelPadding, kPreviewSize + kPanelPadding), true);
	const int keyIndex = FindDisplayedKey();
	if (keyIndex >= 0)
	{
		const std::string& path = clip_.GetKeys()[static_cast<size_t>(keyIndex)].texturePath;
		const unsigned long long texture = FindImGuiTexture(path);
		if (texture != 0)
		{
			const DirectX::TexMetadata& meta = TextureManager::GetInstance()->GetMetadata(path);
			const ImVec2 size = FitInto(static_cast<float>(meta.width), static_cast<float>(meta.height), kPreviewSize);
			ImGui::Image(static_cast<ImTextureID>(texture), size);
		}
		else
		{
			ImGui::TextDisabled("(画像が読めない)");
		}
	}
	else
	{
		ImGui::TextDisabled("(キー無し)");
	}
	ImGui::EndChild();
}

void SpriteAnimationEditor::DrawKeyInspector()
{
	ImGui::BeginChild("##SpriteAnimInspector", ImVec2(kInspectorWidth, kPreviewSize + kPanelPadding + kInspectorExtraHeight), true);
	if (selectedKey_ < 0 || static_cast<size_t>(selectedKey_) >= clip_.GetKeys().size())
	{
		selectedKey_ = -1;
		ImGui::TextDisabled("キーを選ぶとここで編集できる");
		ImGui::EndChild();
		return;
	}

	const size_t index = static_cast<size_t>(selectedKey_);
	ImGui::Text("キー %d", selectedKey_ + 1);

	float time = clip_.GetKeys()[index].time;
	if (ImGui::DragFloat("時刻 (s)", &time, kDragTimeSpeed, 0.0f, 0.0f, "%.2f"))
	{
		selectedKey_ = static_cast<int>(clip_.MoveKey(index, time));
	}

	// 次のキーまでの長さ（最後のキーなら自分の表示時間）。変えると後ろのキーがずれる
	float displayDuration = clip_.GetKeyDisplayDuration(static_cast<size_t>(selectedKey_));
	if (ImGui::DragFloat("表示時間 (s)", &displayDuration, kDragTimeSpeed, kSpriteAnimationMinKeyDuration, kMaxKeyDuration, "%.2f"))
	{
		clip_.SetKeyDuration(static_cast<size_t>(selectedKey_), displayDuration);
	}

	ImGui::TextWrapped("%s", clip_.GetKeys()[static_cast<size_t>(selectedKey_)].texturePath.c_str());
	if (ImGui::Button("画像を変える"))
	{
		RefreshTextureList();
		ImGui::OpenPopup(kKeyTexturePopup);
	}
	std::string newPath;
	if (DrawTexturePicker(kKeyTexturePopup, newPath))
	{
		EnsureTextureLoaded(newPath);
		clip_.SetKeyTexture(static_cast<size_t>(selectedKey_), newPath);
	}
	ImGui::SameLine();
	if (ImGui::Button("削除"))
	{
		clip_.RemoveKey(static_cast<size_t>(selectedKey_));
		selectedKey_ = -1;
	}
	ImGui::EndChild();
}

bool SpriteAnimationEditor::DrawTexturePicker(const char* popupId, std::string& outPath)
{
	bool picked = false;
	if (ImGui::BeginPopup(popupId))
	{
		for (const std::string& relative : textureList_)
		{
			if (ImGui::Selectable(relative.c_str()))
			{
				outPath = std::string(kTextureDirectory) + "/" + relative;
				picked = true;
			}
		}
		if (textureList_.empty())
		{
			ImGui::TextDisabled("(画像が見つからない)");
		}
		ImGui::EndPopup();
	}
	return picked;
}

void SpriteAnimationEditor::DrawTimeline()
{
	const ImGuiStyle& style = ImGui::GetStyle();
	const float childHeight = kRulerHeight + kLaneHeight + style.ScrollbarSize + style.WindowPadding.y * kPaddingSides;
	ImGui::BeginChild("##SpriteAnimTimeline", ImVec2(0.0f, childHeight), true, ImGuiWindowFlags_HorizontalScrollbar);

	const float duration = clip_.GetDuration();
	const float pps = pixelsPerSecond_;
	const float canvasWidth = (std::max)(ImGui::GetContentRegionAvail().x, ((std::max)(duration, time_) + kTailSeconds) * pps);
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const float laneTop = origin.y + kRulerHeight;
	const float laneBottom = laneTop + kLaneHeight;

	ImGui::InvisibleButton("##SpriteAnimCanvas", ImVec2(canvasWidth, kRulerHeight + kLaneHeight));
	const bool isActivated = ImGui::IsItemActivated();
	const bool isActive = ImGui::IsItemActive();
	const bool isHovered = ImGui::IsItemHovered();

	const std::vector<SpriteAnimationKey>& keys = clip_.GetKeys();
	const auto keyX = [&](size_t i) { return origin.x + keys[i].time * pps; };
	// キーの帯の右端。次のキーまで、最後のキーなら自分の表示時間まで
	const auto keyEndX = [&](size_t i)
	{
		return (i + 1 < keys.size()) ? keyX(i + 1) : keyX(i) + keys[i].duration * pps;
	};

	const auto snapTime = [](float t) { return std::floor(t / kSnapSeconds + kRoundHalf) * kSnapSeconds; };

	// 帯の右端（表示時間を変えるつまみ）を探す。次のキーの左端と重なるので、本体より先に見る
	const ImVec2 mouse = ImGui::GetIO().MousePos;
	int edgeKey = -1;
	if (mouse.y >= laneTop && mouse.y < laneBottom)
	{
		for (size_t i = keys.size(); i > 0; --i)
		{
			const size_t index = i - 1;
			const float endX = keyEndX(index);
			// 帯が細いときは、つまみが帯を食い尽くさないよう幅を絞る
			const float edgeWidth = (std::min)(kEdgeGrabWidth, (endX - keyX(index)) * kEdgeGrabRatio);
			if (mouse.x >= endX - edgeWidth && mouse.x <= endX + kEdgeGrabOutside)
			{
				edgeKey = static_cast<int>(index);
				break;
			}
		}
	}

	// マウス位置にあるキーを探す。後ろのキーほど手前に描くので、後ろから見る
	int hitKey = -1;
	for (size_t i = keys.size(); i > 0; --i)
	{
		const size_t index = i - 1;
		const float x = keyX(index);
		const bool onMarker = std::fabs(mouse.x - x) <= kMarkerRadius && mouse.y >= origin.y && mouse.y < laneTop;
		// サムネイルの幅まで掴める。帯の残りはダブルクリックでキーを足すために空けておく
		const float grabWidth = (std::max)((std::min)(kLaneHeight, keyEndX(index) - x), kMarkerRadius * 2.0f);
		const bool onThumb = mouse.x >= x && mouse.x < x + grabWidth && mouse.y >= laneTop && mouse.y < laneBottom;
		if (onMarker || onThumb)
		{
			hitKey = static_cast<int>(index);
			break;
		}
	}

	const float mouseTime = (std::max)((mouse.x - origin.x) / pps, 0.0f);
	if (isActivated)
	{
		if (edgeKey >= 0)
		{
			selectedKey_ = edgeKey;
			dragMode_ = DragMode::ResizeKey;
			const size_t index = static_cast<size_t>(edgeKey);
			dragOffsetSeconds_ = mouseTime - (keys[index].time + clip_.GetKeyDisplayDuration(index));
		}
		else if (hitKey >= 0)
		{
			selectedKey_ = hitKey;
			dragMode_ = DragMode::MoveKey;
			dragOffsetSeconds_ = mouseTime - keys[static_cast<size_t>(hitKey)].time;
		}
		else
		{
			// 何も無いところは再生ヘッドを動かす
			selectedKey_ = -1;
			dragMode_ = DragMode::None;
			isPlaying_ = false;
		}
	}
	if (isActive)
	{
		if (dragMode_ == DragMode::ResizeKey && selectedKey_ >= 0)
		{
			// 右端の位置から表示時間を逆算する。途中のキーなら後ろのキーがついてくる
			const size_t index = static_cast<size_t>(selectedKey_);
			const float endTime = snapTime(mouseTime - dragOffsetSeconds_);
			clip_.SetKeyDuration(index, endTime - keys[index].time);
		}
		else if (dragMode_ == DragMode::MoveKey && selectedKey_ >= 0)
		{
			const float target = (std::max)(mouseTime - dragOffsetSeconds_, 0.0f);
			selectedKey_ = static_cast<int>(clip_.MoveKey(static_cast<size_t>(selectedKey_), snapTime(target)));
		}
		else
		{
			time_ = mouseTime;
		}
	}
	else
	{
		dragMode_ = DragMode::None;
	}
	// つまみの上、または掴んでいる間は横向きの矢印にする
	const bool isResizing = isActive && dragMode_ == DragMode::ResizeKey;
	if (isResizing || (isHovered && !isActive && edgeKey >= 0))
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	}
	if (isHovered && hitKey < 0 && edgeKey < 0 && mouse.y >= laneTop && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
	{
		AddKeyAt(mouseTime);
	}

	// ---- 描画 ----
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 canvasMax(origin.x + canvasWidth, laneBottom);
	drawList->AddRectFilled(origin, canvasMax, kColorBackground);
	drawList->AddRectFilled(origin, ImVec2(canvasMax.x, laneTop), kColorRuler);

	// 目盛り
	const float minorStep = pps >= kFineTickMinPixelsPerSecond ? kMinorTickSecondsFine : kMinorTickSecondsCoarse;
	const float endTime = canvasWidth / pps;
	for (float t = 0.0f; t <= endTime; t += minorStep)
	{
		const float x = origin.x + t * pps;
		const bool isMajor = std::fmod(t + kSnapSeconds * kRoundHalf, kMajorTickSeconds) < minorStep * kRoundHalf;
		drawList->AddLine(ImVec2(x, laneTop - (isMajor ? kTickHeightMajor : kTickHeightMinor)), ImVec2(x, laneTop), kColorTick);
		if (isMajor)
		{
			char label[16];
			std::snprintf(label, sizeof(label), "%.0fs", t);
			drawList->AddText(ImVec2(x + kTickLabelOffset, origin.y + kHoldInset), kColorTickText, label);
		}
	}

	// キーごとに、次のキーまで（空白部分）を同じ画像で埋めて、サムネイルを置く
	for (size_t i = 0; i < keys.size(); ++i)
	{
		const float x0 = keyX(i);
		const float x1 = keyEndX(i);
		const bool isSelected = static_cast<int>(i) == selectedKey_;

		drawList->AddRectFilled(ImVec2(x0, laneTop + kHoldInset), ImVec2(x1, laneBottom - kHoldInset),
			isSelected ? kColorLaneHoldSelected : kColorLaneHold);

		drawList->PushClipRect(ImVec2(x0, laneTop), ImVec2(x1, laneBottom), true);
		const unsigned long long texture = FindImGuiTexture(keys[i].texturePath);
		const float boxSize = kLaneHeight - kThumbPadding * 2.0f;
		if (texture != 0)
		{
			const DirectX::TexMetadata& meta = TextureManager::GetInstance()->GetMetadata(keys[i].texturePath);
			const ImVec2 size = FitInto(static_cast<float>(meta.width), static_cast<float>(meta.height), boxSize);
			const ImVec2 thumbMin(x0 + kThumbPadding, laneTop + kThumbPadding);
			drawList->AddImage(static_cast<ImTextureID>(texture), thumbMin, ImVec2(thumbMin.x + size.x, thumbMin.y + size.y));
		}
		else
		{
			const ImVec2 missMin(x0 + kThumbPadding, laneTop + kThumbPadding);
			drawList->AddRectFilled(missMin, ImVec2(missMin.x + boxSize, missMin.y + boxSize), kColorMissing);
		}
		drawList->PopClipRect();

		drawList->AddRect(ImVec2(x0, laneTop), ImVec2(x1, laneBottom),
			isSelected ? kColorKeySelected : kColorKeyBorder, 0.0f, 0, isSelected ? kSelectedBorderThickness : kBorderThickness);

		// ルーラー側のひし形マーカー
		const float markerY = laneTop - kMarkerRadius - kMarkerGap;
		drawList->AddQuadFilled(ImVec2(x0, markerY - kMarkerRadius), ImVec2(x0 + kMarkerRadius, markerY),
			ImVec2(x0, markerY + kMarkerRadius), ImVec2(x0 - kMarkerRadius, markerY),
			isSelected ? kColorKeySelected : kColorMarker);

		// 右端のつまみ。選んでいるキーと、マウスが乗っているキーに出す
		if (isSelected || static_cast<int>(i) == edgeKey)
		{
			drawList->AddRectFilled(ImVec2(x1 - kEdgeHandleHalfWidth, laneTop), ImVec2(x1 + kEdgeHandleHalfWidth, laneBottom), kColorEdgeHandle);
		}
	}

	// 再生ヘッド
	const float headX = origin.x + time_ * pps;
	drawList->AddLine(ImVec2(headX, origin.y), ImVec2(headX, laneBottom), kColorPlayhead);
	drawList->AddTriangleFilled(ImVec2(headX - kPlayheadTriangle, origin.y), ImVec2(headX + kPlayheadTriangle, origin.y),
		ImVec2(headX, origin.y + kPlayheadTriangle * kPlayheadTriangleScale), kColorPlayhead);

	ImGui::EndChild();
}

void SpriteAnimationEditor::AddKeyAt(float time)
{
	if (pendingTexture_.empty())
	{
		statusMessage_ = "先に「画像を選ぶ」で画像を選んでね";
		return;
	}
	if (!TextureManager::GetInstance()->CheckTextureExists(pendingTexture_))
	{
		statusMessage_ = "画像が見つからない: " + pendingTexture_;
		return;
	}
	EnsureTextureLoaded(pendingTexture_);
	// マウスで置いたときの中途半端な時刻をそろえる
	const float snapped = std::floor(time / kSnapSeconds + kRoundHalf) * kSnapSeconds;
	selectedKey_ = static_cast<int>(clip_.AddKey(snapped, pendingTexture_, newKeyDuration_));
	statusMessage_.clear();
}

std::string SpriteAnimationEditor::BuildFilePath() const
{
	return SpriteAnimationClip::BuildFilePath(nameBuffer_.data());
}

void SpriteAnimationEditor::Save()
{
	if (nameBuffer_[0] == '\0')
	{
		statusMessage_ = "保存名が空";
		return;
	}
	const std::string path = BuildFilePath();
	statusMessage_ = clip_.SaveToFile(path) ? "保存した: " + path : "保存できなかった: " + path;
}

void SpriteAnimationEditor::Load()
{
	const std::string path = BuildFilePath();
	if (!clip_.LoadFromFile(path))
	{
		statusMessage_ = "読み込めなかった（無いか壊れている）: " + path;
		return;
	}
	for (const SpriteAnimationKey& key : clip_.GetKeys())
	{
		EnsureTextureLoaded(key.texturePath);
	}
	selectedKey_ = -1;
	time_ = 0.0f;
	isPlaying_ = false;
	statusMessage_ = "読み込んだ: " + path;
}

void SpriteAnimationEditor::RefreshTextureList()
{
	textureList_.clear();
	const std::filesystem::path dir = PathManager::GetApplicationResourceRoot() / kTextureDirectory;
	std::error_code error;
	if (!std::filesystem::is_directory(dir, error))
	{
		return;
	}
	for (std::filesystem::recursive_directory_iterator it(dir, std::filesystem::directory_options::skip_permission_denied, error), end;
		it != end; it.increment(error))
	{
		if (error)
		{
			error.clear();
			continue;
		}
		if (!it->is_regular_file(error))
		{
			continue;
		}
		const std::string extension = it->path().extension().string();
		if (extension != ".png" && extension != ".jpg" && extension != ".jpeg" && extension != ".bmp" && extension != ".tga")
		{
			continue;
		}
		textureList_.push_back(std::filesystem::relative(it->path(), dir, error).generic_string());
	}
	std::sort(textureList_.begin(), textureList_.end());
}

void SpriteAnimationEditor::EnsureTextureLoaded(const std::string& path) const
{
	// エディタの画像はシーンが変わっても使うので常駐で読む
	TextureManager::GetInstance()->LoadTexture(path, ResourceLifetime::Resident);
}

unsigned long long SpriteAnimationEditor::FindImGuiTexture(const std::string& path) const
{
	uint32_t srvIndex = 0;
	if (!TextureManager::GetInstance()->TryGetTextureIndexByFilePath(path, srvIndex))
	{
		return 0;
	}
	// ImGui も同じ SRV ヒープを使っているので、GPU ハンドルがそのまま画像IDになる
	return static_cast<unsigned long long>(TextureManager::GetInstance()->GetSrvHandleGPU(srvIndex).ptr);
}

int SpriteAnimationEditor::FindDisplayedKey() const
{
	const std::vector<SpriteAnimationKey>& keys = clip_.GetKeys();
	if (keys.empty())
	{
		return -1;
	}
	// 停止中に総秒数の先へ置いたときは、折り返さず最後の画像を出す
	if (time_ >= clip_.GetDuration() && !isPlaying_)
	{
		return static_cast<int>(keys.size()) - 1;
	}
	return clip_.FindKeyIndex(time_);
}
#endif
} // namespace KCE
