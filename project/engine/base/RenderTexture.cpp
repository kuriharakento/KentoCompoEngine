#include "RenderTexture.h"

#include <cassert>
#include "DirectXTex/d3dx12.h"
// system
#include "DirectXCommon.h"
#include "manager/system/SrvManager.h"


void RenderTexture::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor)
{
	dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    width_ = width;
    height_ = height;
    format_ = format;
    clearColor_ = clearColor;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clear{};
    clear.Format = format;
    clear.Color[0] = clearColor.x;
    clear.Color[1] = clearColor.y;
    clear.Color[2] = clearColor.z;
    clear.Color[3] = clearColor.w;

    HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        &clear,
        IID_PPV_ARGS(&texture_));
    assert(SUCCEEDED(hr));

    rtvHeap_ = dxCommon_->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false);
    rtvHandle_ = dxCommon_->GetCPUDescriptorHandle(rtvHeap_.Get(), dxCommon_->GetDescriptorSizeRTV(), 0);

    dxCommon_->GetDevice()->CreateRenderTargetView(texture_.Get(), nullptr, rtvHandle_);

    srvIndex_ = srvManager_->Allocate();
    srvManager_->CreateSRVforTexture2D(srvIndex_, texture_.Get(), format, 1);

    currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
}

void RenderTexture::BeginRender() {
    if (currentState_ != D3D12_RESOURCE_STATE_RENDER_TARGET) {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            texture_.Get(),
            currentState_,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
        currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVHandle();
    dxCommon_->GetCommandList()->OMSetRenderTargets(1, &rtvHandle_, FALSE, &dsvHandle);

    // ビューポートとシザー設定
    D3D12_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(width_);
    viewport.Height = static_cast<float>(height_);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect{};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = static_cast<LONG>(width_);
    scissorRect.bottom = static_cast<LONG>(height_);

    dxCommon_->GetCommandList()->RSSetViewports(1, &viewport);
    dxCommon_->GetCommandList()->RSSetScissorRects(1, &scissorRect);

    float clearColor[] = { clearColor_.x, clearColor_.y, clearColor_.z, clearColor_.w };
    dxCommon_->GetCommandList()->ClearRenderTargetView(rtvHandle_, clearColor, 0, nullptr);
}

void RenderTexture::EndRender() {
    if (currentState_ != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            texture_.Get(),
            currentState_,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
        currentState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
}

void RenderTexture::PreDrawForImGui()
{
    if (currentState_ != D3D12_RESOURCE_STATE_RENDER_TARGET)
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            texture_.Get(),
            currentState_,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
        currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVHandle();
    dxCommon_->GetCommandList()->OMSetRenderTargets(1, &rtvHandle_, FALSE, &dsvHandle);

    // ビューポートとシザー設定
    D3D12_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(width_);
    viewport.Height = static_cast<float>(height_);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect{};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = static_cast<LONG>(width_);
    scissorRect.bottom = static_cast<LONG>(height_);

    dxCommon_->GetCommandList()->RSSetViewports(1, &viewport);
    dxCommon_->GetCommandList()->RSSetScissorRects(1, &scissorRect);
}

void RenderTexture::PostDrawForImGui()
{
    if (currentState_ != D3D12_RESOURCE_STATE_RENDER_TARGET)
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            texture_.Get(),
            currentState_,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
        currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
}

D3D12_GPU_DESCRIPTOR_HANDLE RenderTexture::GetGPUHandle() const {
    return srvManager_->GetGPUDescriptorHandle(srvIndex_);
}
