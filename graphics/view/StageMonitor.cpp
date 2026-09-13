#include "graphics/view/StageMonitor.h"

#include "base/Camera.h"
#include "base/RenderTexture.h"
#include "gameobject/base/GameObject.h"
#include "graphics/3d/Object3d.h"
#include "graphics/view/ISubViewProvider.h"
#include "graphics/view/RenderView.h"
#include "manager/scene/CameraManager.h"

namespace KCE
{
namespace
{
// 作ったフレームの描画で初めて映像ができるので、その次の Update から差す
constexpr uint32_t kUpdatesBeforeBind = 2;

Model* GetScreenModel(GameObject* screen)
{
	if (!screen)
	{
		return nullptr;
	}
	auto* object3d = dynamic_cast<Object3d*>(screen->GetRenderable3d());
	return object3d ? object3d->GetModel() : nullptr;
}
} // namespace

StageMonitor::~StageMonitor()
{
	Finalize();
}

bool StageMonitor::Initialize(ISubViewProvider* provider, CameraManager* cameraManager, const std::string& cameraName, uint32_t width, uint32_t height)
{
	if (!provider || !cameraManager || width == 0 || height == 0)
	{
		return false;
	}

	cameraManager->AddCamera(cameraName);
	camera_ = cameraManager->GetCamera(cameraName);
	if (!camera_)
	{
		return false;
	}
	camera_->SetAspectRatio(static_cast<float>(width) / static_cast<float>(height));

	provider_ = provider;
	view_ = provider_->CreateSubView(cameraName, width, height);
	if (!view_)
	{
		return false;
	}
	view_->SetCamera(camera_);
	// 画面自身を映すと、読んでいるテクスチャに描くことになる
	view_->SetLayerMask(kRenderLayerAll & ~kScreenLayer);
	// モニターは客に見せる映像なので、デバッグ用の線は映さない
	view_->SetPassEnabled(RenderViewPass::DebugLines, false);
	// モニターは毎フレーム描き直さなくても分からないので、テレビの速さに間引く
	SetFramesPerSecond(kDefaultFramesPerSecond);
	updateCount_ = 0;
	return true;
}

void StageMonitor::SetFramesPerSecond(float framesPerSecond)
{
	if (view_)
	{
		view_->SetUpdateInterval(framesPerSecond > 0.0f ? 1.0f / framesPerSecond : 0.0f);
	}
}

void StageMonitor::SetScreen(GameObject* screen)
{
	BindScreenTexture(false);
	screen_ = screen;
	if (screen_)
	{
		screen_->SetRenderLayer(kScreenLayer);
	}
}

void StageMonitor::Update()
{
	if (updateCount_ < kUpdatesBeforeBind)
	{
		++updateCount_;
	}
	if (!screenBound_ && updateCount_ >= kUpdatesBeforeBind)
	{
		BindScreenTexture(true);
	}
}

void StageMonitor::Finalize()
{
	BindScreenTexture(false);
	screen_ = nullptr;
	if (provider_ && view_)
	{
		provider_->DestroySubView(view_);
	}
	view_ = nullptr;
	provider_ = nullptr;
	camera_ = nullptr;
}

void StageMonitor::BindScreenTexture(bool bind)
{
	Model* model = GetScreenModel(screen_);
	if (!model || !view_ || !view_->GetSceneColor())
	{
		screenBound_ = false;
		return;
	}
	model->SetOverrideTexture(bind ? view_->GetSceneColor()->GetGPUHandle() : D3D12_GPU_DESCRIPTOR_HANDLE{});
	screenBound_ = bind;
}
} // namespace KCE
