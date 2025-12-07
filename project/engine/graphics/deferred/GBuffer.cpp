#include "GBuffer.h"
#include "base/DirectXCommon.h"
#include "manager/system/SrvManager.h"
#include "base/Logger.h"
#include <cassert>

GBuffer::~GBuffer() {
    // ComPtrで自動解放
}

void GBuffer::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height) {
    assert(dxCommon);
    assert(srvManager);
    
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    width_ = width;
    height_ = height;
    
    CreateRenderTargets();
    CreateDepthBuffer();
    CreateDescriptors();
    
    Logger::Log("GBuffer initialized: " + std::to_string(width) + "x" + std::to_string(height) + "\n");
}

void GBuffer::CreateRenderTargets() {
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
    for (uint32_t i = 0; i < GBufferIndex::Count; ++i) {
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
        srvIndices_[i] = srvManager_->Allocate();
        srvManager_->CreateSRVforTexture2D(srvIndices_[i], renderTargets_[i].Get(), formats[i], 1);
    }
}

void GBuffer::CreateDepthBuffer() {
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
    resourceDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    
    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = kDepthFormat;
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
    dsvDesc.Format = kDepthFormat;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    
    device->CreateDepthStencilView(depthBuffer_.Get(), &dsvDesc, dsvHandle_);
    
    // 深度バッファのSRV作成
    depthSrvIndex_ = srvManager_->Allocate();
    srvManager_->CreateSRVforTexture2D(depthSrvIndex_, depthBuffer_.Get(), DXGI_FORMAT_R32_FLOAT, 1);
}

void GBuffer::CreateDescriptors() {
    // 既にCreateRenderTargetsとCreateDepthBufferで作成済み
}

void GBuffer::BeginGeometryPass() {
    auto* commandList = dxCommon_->GetCommandList();
    
    // 全レンダーターゲットをRENDER_TARGET状態に遷移、深度バッファをDEPTH_WRITE状態に遷移
    D3D12_RESOURCE_BARRIER barriers[GBufferIndex::Count + 1];
    for (uint32_t i = 0; i < GBufferIndex::Count; ++i) {
        barriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[i].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barriers[i].Transition.pResource = renderTargets_[i].Get();
        barriers[i].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers[i].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    }
    
    // 深度バッファをDEPTH_WRITE状態に遷移
    barriers[GBufferIndex::Count].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[GBufferIndex::Count].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barriers[GBufferIndex::Count].Transition.pResource = depthBuffer_.Get();
    barriers[GBufferIndex::Count].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barriers[GBufferIndex::Count].Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barriers[GBufferIndex::Count].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    
    commandList->ResourceBarrier(GBufferIndex::Count + 1, barriers);
    
    // レンダーターゲットをセット
    commandList->OMSetRenderTargets(GBufferIndex::Count, rtvHandles_.data(), FALSE, &dsvHandle_);
    
    // クリア
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    for (uint32_t i = 0; i < GBufferIndex::Count; ++i) {
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

void GBuffer::EndGeometryPass() {
    auto* commandList = dxCommon_->GetCommandList();
    
    // 全レンダーターゲットと深度バッファをPIXEL_SHADER_RESOURCE状態に遷移
    D3D12_RESOURCE_BARRIER barriers[GBufferIndex::Count + 1];
    for (uint32_t i = 0; i < GBufferIndex::Count; ++i) {
        barriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[i].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barriers[i].Transition.pResource = renderTargets_[i].Get();
        barriers[i].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[i].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    }
    
    // 深度バッファもSRV状態に遷移
    barriers[GBufferIndex::Count].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[GBufferIndex::Count].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barriers[GBufferIndex::Count].Transition.pResource = depthBuffer_.Get();
    barriers[GBufferIndex::Count].Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barriers[GBufferIndex::Count].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barriers[GBufferIndex::Count].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    
    commandList->ResourceBarrier(GBufferIndex::Count + 1, barriers);
}


uint32_t GBuffer::GetSRVIndex(uint32_t index) const {
    if (index >= GBufferIndex::Count) {
        return 0;
    }
    return srvIndices_[index];
}
