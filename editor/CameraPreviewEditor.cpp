#include "CameraPreviewEditor.h"

#include <algorithm>
#include <cmath>

#include "base/Camera.h"
#include "base/RenderTexture.h"
#include "graphics/view/ISubViewProvider.h"
#include "graphics/view/RenderView.h"
#include "manager/editor/DebugUIManager.h"
#include "manager/scene/CameraManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace KCE
{
namespace
{
constexpr const char* kWindowTitle = "カメラプレビュー";
// 解像度の刻み。窓を少し動かすたびに作り直さないよう丸める
constexpr uint32_t kSizeStep = 32;
constexpr uint32_t kMinSize = 64;
constexpr uint32_t kMaxSize = 1920;
// 欲しい解像度がこのフレーム数変わらなければ、ビューを作り直す
constexpr int kStableFramesToResize = 20;
// ビューを作るときの初めの解像度
constexpr uint32_t kInitialWidth = 320;
constexpr uint32_t kInitialHeight = 180;

[[maybe_unused]] uint32_t RoundToStep(float value)
{
	const float clamped = (std::clamp)(value, static_cast<float>(kMinSize), static_cast<float>(kMaxSize));
	return static_cast<uint32_t>(std::lround(clamped / kSizeStep)) * kSizeStep;
}
}

CameraPreviewEditor::CameraPreviewEditor() = default;

CameraPreviewEditor::~CameraPreviewEditor()
{
	Finalize();
}

void CameraPreviewEditor::Initialize(ISubViewProvider* provider, CameraManager* cameraManager)
{
#ifdef USE_IMGUI
	provider_ = provider;
	cameraManager_ = cameraManager;
	if (!provider_ || !cameraManager_ || initialized_)
	{
		return;
	}
	initialized_ = true;

	// 最初の1枚は閉じておく。メニューの「ウィンドウ」から開く
	AddWindow(false);
	DebugUIManager::GetInstance()->RegisterDialog(this, [this]() { BeginFrame(); });
#else
	(void)provider;
	(void)cameraManager;
#endif
}

void CameraPreviewEditor::Finalize()
{
#ifdef USE_IMGUI
	if (!initialized_)
	{
		return;
	}
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->Unregister(this);
	}
	if (cameraManager_ && sceneViewCamera_ && cameraManager_->GetViewCameraOverride() == sceneViewCamera_)
	{
		cameraManager_->ClearViewCameraOverride();
	}
	sceneViewCamera_ = nullptr;
	for (auto& target : targets_)
	{
		ReleaseView(*target);
	}
	targets_.clear();
	initialized_ = false;
#endif
}

void CameraPreviewEditor::OpenPreview(const std::string& cameraName)
{
#ifdef USE_IMGUI
	if (!initialized_)
	{
		return;
	}
	DebugUIManager* debugUI = DebugUIManager::GetInstance();
	// 閉じている窓があれば使い回す
	for (auto& target : targets_)
	{
		if (!debugUI->IsWindowVisible(target->windowName))
		{
			target->cameraName = cameraName;
			debugUI->SetWindowVisible(target->windowName, true);
			return;
		}
	}
	pendingAdd_ = true;
	pendingAddCamera_ = cameraName;
#else
	(void)cameraName;
#endif
}

CameraPreviewEditor::PreviewTarget& CameraPreviewEditor::AddWindow(bool visible)
{
	auto target = std::make_unique<PreviewTarget>();
	target->index = nextIndex_++;
	target->windowName = std::string(kWindowTitle) + " " + std::to_string(target->index) + "###CameraPreview" + std::to_string(target->index);
	PreviewTarget& ref = *target;
	targets_.push_back(std::move(target));
	// 勝手にドッキングされず、画面の真ん中に浮かせて開く
	DebugUIManager::GetInstance()->RegisterWindow(this, ref.windowName, [this, &ref]() { DrawWindow(ref); }, EditorDock::Floating, visible);
	return ref;
}

void CameraPreviewEditor::BeginFrame()
{
	DebugUIManager* debugUI = DebugUIManager::GetInstance();
	// ウィンドウの描画（Draw）より前に呼ばれる。ここで全部止めて、描かれた窓だけ DrawWindow が有効に戻す。
	// 閉じた窓やタブの裏の窓は DrawWindow が呼ばれず、描画が止まったままになる
	for (auto& target : targets_)
	{
		if (!target->view)
		{
			continue;
		}
		if (!debugUI->IsWindowVisible(target->windowName))
		{
			ReleaseView(*target);
			continue;
		}
		target->view->SetEnabled(false);
	}

	if (pendingAdd_)
	{
		pendingAdd_ = false;
		PreviewTarget& added = AddWindow(true);
		added.cameraName = pendingAddCamera_;
		pendingAddCamera_.clear();
	}
}

void CameraPreviewEditor::DrawWindow(PreviewTarget& target)
{
#ifdef USE_IMGUI
	// --- カメラ選択 ---
	const std::vector<std::string> names = cameraManager_->GetCameraNames();
	const char* preview = target.cameraName.empty() ? "(カメラを選ぶ)" : target.cameraName.c_str();
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
	if (ImGui::BeginCombo("##Camera", preview))
	{
		for (const std::string& name : names)
		{
			const bool selected = name == target.cameraName;
			if (ImGui::Selectable(name.c_str(), selected))
			{
				target.cameraName = name;
			}
			if (selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	Camera* camera = target.cameraName.empty() ? nullptr : cameraManager_->GetCamera(target.cameraName);

	ImGui::SameLine();
	if (ImGui::Button("+"))
	{
		pendingAdd_ = true;
		pendingAddCamera_ = target.cameraName;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("プレビューの窓を増やす");
	}

	// Scene ビューをこのカメラに切り替える（もう一度押すと戻す）
	ImGui::SameLine();
	const bool viewingInScene = camera && sceneViewCamera_ == camera && cameraManager_->GetViewCameraOverride() == camera;
	ImGui::BeginDisabled(!camera);
	if (ImGui::Button(viewingInScene ? "Scene を戻す" : "Scene で見る"))
	{
		if (viewingInScene)
		{
			cameraManager_->ClearViewCameraOverride();
			sceneViewCamera_ = nullptr;
		}
		else
		{
			cameraManager_->SetViewCameraOverride(camera);
			sceneViewCamera_ = camera;
		}
	}
	ImGui::EndDisabled();

	if (!camera)
	{
		// カメラが消えたときに、ビューのカメラが nullptr（=アクティブカメラを描く）にならないよう止めておく
		if (target.view)
		{
			target.view->SetEnabled(false);
		}
		ImGui::TextDisabled("%s", target.cameraName.empty() ? "カメラを選ぶと映像が出ます" : "カメラが見つかりません");
		return;
	}

	// --- 画像の大きさ：カメラの縦横比を保って、窓に収まる一番大きい大きさ ---
	const ImVec2 available = ImGui::GetContentRegionAvail();
	if (available.x < 1.0f || available.y < 1.0f)
	{
		return;
	}
	const float aspect = (std::max)(camera->GetAspectRatio(), 0.01f);
	ImVec2 imageSize = available;
	if (available.x / available.y > aspect)
	{
		imageSize.x = available.y * aspect;
	}
	else
	{
		imageSize.y = available.x / aspect;
	}

	// --- 解像度：欲しい大きさが落ち着いてからビューを作り直す ---
	const uint32_t wantedWidth = RoundToStep(imageSize.x);
	const uint32_t wantedHeight = RoundToStep(imageSize.y);
	if (!target.view)
	{
		RecreateView(target, kInitialWidth, kInitialHeight);
		target.wantedWidth = wantedWidth;
		target.wantedHeight = wantedHeight;
		target.stableFrames = 0;
	}
	else if (wantedWidth != target.wantedWidth || wantedHeight != target.wantedHeight)
	{
		target.wantedWidth = wantedWidth;
		target.wantedHeight = wantedHeight;
		target.stableFrames = 0;
	}
	else if (target.stableFrames < kStableFramesToResize)
	{
		++target.stableFrames;
	}
	else if (wantedWidth != target.viewWidth || wantedHeight != target.viewHeight)
	{
		RecreateView(target, wantedWidth, wantedHeight);
	}

	if (!target.view || !target.view->GetSceneColor())
	{
		return;
	}
	target.view->SetCamera(camera);
	target.view->SetEnabled(true);

	// 窓の真ん中に置く
	const ImVec2 cursor = ImGui::GetCursorPos();
	ImGui::SetCursorPos(ImVec2(cursor.x + (available.x - imageSize.x) * 0.5f, cursor.y + (available.y - imageSize.y) * 0.5f));
	ImGui::Image(static_cast<ImTextureID>(target.view->GetSceneColor()->GetGPUHandle().ptr), imageSize);
#else
	(void)target;
#endif
}

void CameraPreviewEditor::RecreateView(PreviewTarget& target, uint32_t width, uint32_t height)
{
	// 破棄は provider が次のフレームの頭まで遅らせるので、GPU の完了待ちは要らない
	ReleaseView(target);
	target.view = provider_->CreateSubView(std::string(kWindowTitle) + " " + std::to_string(target.index), width, height);
	target.viewWidth = width;
	target.viewHeight = height;
}

void CameraPreviewEditor::ReleaseView(PreviewTarget& target)
{
	if (target.view && provider_)
	{
		provider_->DestroySubView(target.view);
	}
	target.view = nullptr;
	target.viewWidth = 0;
	target.viewHeight = 0;
	target.stableFrames = 0;
}
} // namespace KCE
