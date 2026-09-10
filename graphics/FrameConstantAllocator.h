#pragma once

#include <cstddef>
#include <cstdint>
#include <wrl.h>
#include <d3d12.h>

namespace KCE
{
class DirectXCommon;

/** @brief 1フレーム中の描画定数を重ならない領域へ順番に配置する */
class FrameConstantAllocator
{
public:
	struct Allocation
	{
		void* cpuAddress = nullptr;
		D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0;
	};

	bool Initialize(DirectXCommon* dxCommon, size_t capacity);
	void BeginFrame();
	Allocation Allocate(size_t size);
	size_t GetCapacity() const { return capacity_; }

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
	uint8_t* mappedAddress_ = nullptr;
	size_t capacity_ = 0;
	size_t offset_ = 0;
	// このフレームで容量超過を既に報告したか。
	// 超過は描画のたびに起きるため、毎回ログを出すと1フレームで数百行になりログが埋まる。
	bool overflowReportedThisFrame_ = false;
};
} // namespace KCE
