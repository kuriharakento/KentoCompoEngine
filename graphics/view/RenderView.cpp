#include "graphics/view/RenderView.h"

#include "base/RenderTexture.h"
#include "graphics/deferred/GBuffer.h"

namespace KCE
{
namespace
{
/** @brief シーンの背景色 */
constexpr float kClearColorValue = 0.1f;
} // namespace

RenderView::RenderView() = default;
RenderView::~RenderView() = default;

void RenderView::Initialize(
	DirectXCommon* dxCommon,
	SrvManager* srvManager,
	const std::string& name,
	uint32_t width,
	uint32_t height,
	DXGI_FORMAT colorFormat)
{
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	name_ = name;
	width_ = width;
	height_ = height;

	// このビュー専用のG-Buffer。解像度ごとに必要なので共有できない。
	gBuffer_ = std::make_unique<GBuffer>();
	gBuffer_->Initialize(dxCommon, srvManager, width, height);

	const Vector4 clearColor = { kClearColorValue, kClearColorValue, kClearColorValue, 1.0f };
	sceneColor_ = std::make_unique<RenderTexture>();
	sceneColor_->Initialize(dxCommon, srvManager, width, height, colorFormat, clearColor);

	// 発光バッファは毎フレーム黒でクリアする。何も書かなければブルームは出ない
	const Vector4 bloomClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	bloomMask_ = std::make_unique<RenderTexture>();
	bloomMask_->Initialize(dxCommon, srvManager, width, height, colorFormat, bloomClearColor);
}

void RenderView::Resize(uint32_t width, uint32_t height)
{
	if (!IsValid() || (width == width_ && height == height_))
	{
		return;
	}

	width_ = width;
	height_ = height;

	gBuffer_->Resize(width, height);
	sceneColor_->Resize(width, height);
	bloomMask_->Resize(width, height);
}
} // namespace KCE
