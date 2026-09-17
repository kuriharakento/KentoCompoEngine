#include "graphics/view/StageMonitor.h"

#include <algorithm>

#include "base/Camera.h"
#include "base/RenderTexture.h"
#include "gameobject/base/GameObject.h"
#include "graphics/3d/Object3d.h"
#include "graphics/view/ISubViewProvider.h"
#include "graphics/view/RenderView.h"
#include "manager/scene/CameraManager.h"
#include "math/Frustum.h"

namespace KCE
{
namespace
{
// 作ったフレームの描画で初めて映像ができるので、その次の Update から差す
constexpr uint32_t kUpdatesBeforeBind = 2;
// application/Resources/models/plane/plane.gltf の POSITION accessor に記録された範囲
constexpr float kScreenVertexExtentXY = 1.0000003576278687f;
constexpr float kScreenVertexExtentZ = 4.371138828673793e-08f;

Vector3 TransformPoint(const Vector3& point, const Matrix4x4& matrix)
{
	return {
		point.x * matrix.m[0][0] + point.y * matrix.m[1][0] + point.z * matrix.m[2][0] + matrix.m[3][0],
		point.x * matrix.m[0][1] + point.y * matrix.m[1][1] + point.z * matrix.m[2][1] + matrix.m[3][1],
		point.x * matrix.m[0][2] + point.y * matrix.m[1][2] + point.z * matrix.m[2][2] + matrix.m[3][2],
	};
}

AABB MakeScreenWorldAabb(const Matrix4x4& world)
{
	constexpr Vector3 localMin = { -kScreenVertexExtentXY, -kScreenVertexExtentXY, -kScreenVertexExtentZ };
	constexpr Vector3 localMax = { kScreenVertexExtentXY, kScreenVertexExtentXY, kScreenVertexExtentZ };
	Vector3 worldMin = TransformPoint(localMin, world);
	Vector3 worldMax = worldMin;
	for (uint32_t corner = 1; corner < 8; ++corner)
	{
		const Vector3 local = {
			(corner & 1) != 0 ? localMax.x : localMin.x,
			(corner & 2) != 0 ? localMax.y : localMin.y,
			(corner & 4) != 0 ? localMax.z : localMin.z,
		};
		const Vector3 transformed = TransformPoint(local, world);
		worldMin.x = (std::min)(worldMin.x, transformed.x);
		worldMin.y = (std::min)(worldMin.y, transformed.y);
		worldMin.z = (std::min)(worldMin.z, transformed.z);
		worldMax.x = (std::max)(worldMax.x, transformed.x);
		worldMax.y = (std::max)(worldMax.y, transformed.y);
		worldMax.z = (std::max)(worldMax.z, transformed.z);
	}
	return { worldMin, worldMax };
}

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

	const bool cameraAlreadyExists = cameraManager->GetCamera(cameraName) != nullptr;
	cameraManager->AddCamera(cameraName);
	camera_ = cameraManager->GetCamera(cameraName);
	if (!camera_)
	{
		return false;
	}
	camera_->SetAspectRatio(static_cast<float>(width) / static_cast<float>(height));

	provider_ = provider;
	cameraManager_ = cameraManager;
	cameraName_ = cameraName;
	ownsCamera_ = !cameraAlreadyExists;
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

bool StageMonitor::RecreateView(uint32_t width, uint32_t height)
{
	if (!provider_ || !camera_ || width == 0 || height == 0)
	{
		return false;
	}
	// Update の時点では前フレームの GPU 完了待ちが済んでいる。先に参照を外して古い RT を破棄する
	BindScreenTexture(false);
	if (view_)
	{
		provider_->DestroySubView(view_);
	}
	view_ = provider_->CreateSubView(cameraName_, width, height);
	if (!view_)
	{
		return false;
	}
	camera_->SetAspectRatio(static_cast<float>(width) / static_cast<float>(height));
	view_->SetCamera(camera_);
	view_->SetLayerMask(kRenderLayerAll & ~kScreenLayer);
	view_->SetPassEnabled(RenderViewPass::DebugLines, false);
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

void StageMonitor::AttachView(RenderView* view)
{
	BindScreenTexture(false);
	view_ = view;
	updateCount_ = 0;
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

bool StageMonitor::IsScreenVisible(const Frustum& frustum) const
{
	// 画面がまだ無いときは、誤って必要なビューを止めないよう描く側へ倒す
	return !screen_ || frustum.Intersects(MakeScreenWorldAabb(screen_->GetWorldMatrix()));
}

void StageMonitor::SetViewVisible(bool visible)
{
	if (!view_ || view_->IsEnabled() == visible)
	{
		return;
	}
	view_->SetEnabled(visible);
	if (visible)
	{
		view_->RequestImmediateUpdate();
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
	if (ownsCamera_ && cameraManager_)
	{
		cameraManager_->RemoveCamera(cameraName_);
	}
	cameraManager_ = nullptr;
	cameraName_.clear();
	ownsCamera_ = false;
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
