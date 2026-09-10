#include "graphics/FrameConstantAllocator.h"

#include "base/DirectXCommon.h"
#include "base/Logger.h"

namespace KCE
{
namespace
{
constexpr size_t kConstantBufferAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
}

bool FrameConstantAllocator::Initialize(DirectXCommon* dxCommon, size_t capacity)
{
	capacity_ = (capacity + kConstantBufferAlignment - 1) & ~(kConstantBufferAlignment - 1);
	resource_ = dxCommon->CreateBufferResource(capacity_);
	if (!resource_ || FAILED(resource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedAddress_))))
	{
		Logger::Log("フレーム定数バッファを初期化できませんでした。\n", Logger::LogLevel::Error);
		resource_.Reset();
		mappedAddress_ = nullptr;
		return false;
	}
	return true;
}

void FrameConstantAllocator::BeginFrame()
{
	offset_ = 0;
	overflowReportedThisFrame_ = false;
}

FrameConstantAllocator::Allocation FrameConstantAllocator::Allocate(size_t size)
{
	const size_t alignedSize = (size + kConstantBufferAlignment - 1) & ~(kConstantBufferAlignment - 1);
	if (!mappedAddress_ || alignedSize > capacity_ - offset_)
	{
		// 超過はフレーム内の以降の描画でも続けて起きるので、報告はフレームごとに1回に抑える
		if (!overflowReportedThisFrame_)
		{
			Logger::Log("フレーム定数バッファの容量を超えたため、以降のオブジェクトの描画をスキップします。\n", Logger::LogLevel::Error);
			overflowReportedThisFrame_ = true;
		}
		return {};
	}

	Allocation allocation;
	allocation.cpuAddress = mappedAddress_ + offset_;
	allocation.gpuAddress = resource_->GetGPUVirtualAddress() + offset_;
	offset_ += alignedSize;
	return allocation;
}
} // namespace KCE
