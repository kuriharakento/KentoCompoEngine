#include "GBuffer.h"
#include "base/DirectXCommon.h"
#include "manager/system/SrvManager.h"
#include "base/Logger.h"
#include <cassert>

namespace KCE
{
GBuffer::~GBuffer()
{
	// ComPtrで自動解放
}

void GBuffer::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height)
{
	assert(dxCommon);
	assert(srvManager);

	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	width_ = width;
	height_ = height;

	CreateRenderTargets();
	CreateDepthBuffer();
	CreateDescriptors();

	KCE::Logger::Log("GBuffer initialized: " + std::to_string(width) + "x" + std::to_string(height) + "\n");
}

void GBuffer::CreateRenderTargets()
{
	auto* device = dxCommon_->GetDevice();

	// RTVディスクリプタヒープの作成
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.NumDescriptors = GBufferIndex::Count;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap_));
	assert(SUCCEEDED(hr));

	rtvDescriptorSize_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// 各G-Bufferのフォーマット
	DXGI_FORMAT formats[GBufferIndex::Count] = {
		kAlbedoFormat,
		kNormalFormat,
		kMaterialFormat,
		kEmissiveFormat
	};

	// 各レンダーターゲットの作成
	for (uint32_t i = 0; i < GBufferIndex::Count; ++i)
	{
		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = width_;
		resourceDesc.Height = height_;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = formats[i];
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = formats[i];
		clearValue.Color[0] = 0.0f;
		clearValue.Color[1] = 0.0f;
		clearValue.Color[2] = 0.0f;
		clearValue.Color[3] = 0.0f;

		hr = device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&clearValue,
			IID_PPV_ARGS(&renderTargets_[i])
		);
		assert(SUCCEEDED(hr));

		// RTVハンドルの設定
		rtvHandles_[i].ptr = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart().ptr + i * rtvDescriptorSize_;

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = formats[i];
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

		device->CreateRenderTargetView(renderTargets_[i].Get(), &rtvDesc, rtvHandles_[i]);

		// SRVの作成
		if (srvIndices_[i] == 0)
		{
			srvIndices_[i] = srvManager_->Allocate();
		}
		srvManager_->CreateSRVforTexture2D(srvIndices_[i], renderTargets_[i].Get(), formats[i], 1);
	}
}

void GBuffer::CreateDepthBuffer()
{
	auto* device = dxCommon_->GetDevice();

	// DSVディスクリプタヒープの作成
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvDescriptorHeap_));
	assert(SUCCEEDED(hr));

	// 深度バッファリソースの作成
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Width = width_;
	resourceDesc.Height = height_;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	hr = device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, // 初期状態：BeginGeometryPassが期待する状態
		&clearValue,
		IID_PPV_ARGS(&depthBuffer_)
	);
	assert(SUCCEEDED(hr));

	// DSVの作成
	dsvHandle_ = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	device->CreateDepthStencilView(depthBuffer_.Get(), &dsvDesc, dsvHandle_);

	// 深度バッファのSRV作成
	if (depthSrvIndex_ == 0)
	{
		depthSrvIndex_ = srvManager_->Allocate();
	}
	srvManager_->CreateSRVforTexture2D(depthSrvIndex_, depthBuffer_.Get(), DXGI_FORMAT_R24_UNORM_X8_TYPELESS, 1);
}

void GBuffer::TransitionDepthToDepthWrite()
{
	auto* commandList = dxCommon_->GetCommandList();
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = depthBuffer_.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);
}

void GBuffer::TransitionDepthToSRV()
{
	auto* commandList = dxCommon_->GetCommandList();
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = depthBuffer_.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);
}

void GBuffer::CreateDescriptors()
{
	// 既にCreateRenderTargetsとCreateDepthBufferで作成済み
}

void GBuffer::BeginGeometryPass()
{
	auto* commandList = dxCommon_->GetCommandList();

	// 深度バッファをDEPTH_WRITE状態に遷移
	std::vector<D3D12_RESOURCE_BARRIER> activeBarriers;
	activeBarriers.reserve(GBufferIndex::Count + 1);

	for (uint32_t i = 0; i < GBufferIndex::Count; ++i)
	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = renderTargets_[i].Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		activeBarriers.push_back(barrier);
	}

	// 深度バッファの遷移 (SRV -> DepthWrite)
	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = depthBuffer_.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		activeBarriers.push_back(barrier);
	}

	commandList->ResourceBarrier(static_cast<UINT>(activeBarriers.size()), activeBarriers.data());

	// レンダーターゲットをセット
	commandList->OMSetRenderTargets(GBufferIndex::Count, rtvHandles_.data(), FALSE, &dsvHandle_);

	// クリア
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	for (uint32_t i = 0; i < GBufferIndex::Count; ++i)
	{
		commandList->ClearRenderTargetView(rtvHandles_[i], clearColor, 0, nullptr);
	}
	commandList->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);


	// ビューポートとシザー矩形
	D3D12_VIEWPORT viewport = {};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(width_);
	viewport.Height = static_cast<float>(height_);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	commandList->RSSetViewports(1, &viewport);

	D3D12_RECT scissorRect = {};
	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = static_cast<LONG>(width_);
	scissorRect.bottom = static_cast<LONG>(height_);
	commandList->RSSetScissorRects(1, &scissorRect);
}

void GBuffer::EndGeometryPass()
{
	auto* commandList = dxCommon_->GetCommandList();

	// 全レンダーターゲットと深度バッファをPIXEL_SHADER_RESOURCE状態に遷移
	std::vector<D3D12_RESOURCE_BARRIER> activeBarriers;
	activeBarriers.reserve(GBufferIndex::Count + 1);

	for (uint32_t i = 0; i < GBufferIndex::Count; ++i)
	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = renderTargets_[i].Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		activeBarriers.push_back(barrier);
	}

	// 深度バッファもSRV状態に遷移 (DepthWrite -> SRV)
	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = depthBuffer_.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		activeBarriers.push_back(barrier);
	}

	commandList->ResourceBarrier(static_cast<UINT>(activeBarriers.size()), activeBarriers.data());
}


uint32_t GBuffer::GetSRVIndex(uint32_t index) const
{
	if (index >= GBufferIndex::Count)
	{
		return 0;
	}
	return srvIndices_[index];
}

void GBuffer::Resize(uint32_t width, uint32_t height)
{
	width_ = width;
	height_ = height;

	// 既存リソースを解放する
	for (auto& rt : renderTargets_)
	{
		rt.Reset();
	}
	depthBuffer_.Reset();
	rtvDescriptorHeap_.Reset();
	dsvDescriptorHeap_.Reset();

	// 再生成する
	CreateRenderTargets();
	CreateDepthBuffer();
}
} // namespace KCE
