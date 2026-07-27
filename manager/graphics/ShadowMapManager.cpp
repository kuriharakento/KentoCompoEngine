#include "ShadowMapManager.h"
#include "manager/system/SrvManager.h"
#include "base/Logger.h"
#include "DirectXTex/d3dx12.h"
#include <cassert>

ShadowMapManager::~ShadowMapManager() {
    // リソースはComPtrで自動解放されるため、特別な処理は不要
}

void ShadowMapManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    assert(dxCommon);
    assert(srvManager);
    
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    
    // DSVディスクリプタサイズを取得
    dsvDescriptorSize_ = dxCommon_->GetDescriptorSizeDSV();
    
    // シャドウマップ用DSVディスクリプタヒープの作成
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = kMaxDSVCount;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    
    HRESULT hr = dxCommon_->GetDevice()->CreateDescriptorHeap(
        &dsvHeapDesc,
        IID_PPV_ARGS(&dsvDescriptorHeap_)
    );
    assert(SUCCEEDED(hr));
    
    // ディレクショナルライトシャドウマップを初期状態で無効化
    directionalLightShadowMap_.isEnabled = false;
    
    KCE::Logger::Log("ShadowMapManager initialized\n");
}

void ShadowMapManager::CreateDirectionalLightShadowMap(uint32_t resolution) {
    // 深度バッファの作成
    directionalLightShadowMap_.depthBuffer = CreateDepthBuffer(resolution, resolution);
    directionalLightShadowMap_.resolution = resolution;
    directionalLightShadowMap_.type = ShadowMapType::Directional;
    
    // DSVの作成
    directionalLightShadowMap_.dsvHandle = CreateDSV(directionalLightShadowMap_.depthBuffer.Get());
    
    // SRVの作成
    directionalLightShadowMap_.srvIndex = CreateSRV(directionalLightShadowMap_.depthBuffer.Get());
    
    // 有効化
    directionalLightShadowMap_.isEnabled = true;
    
    KCE::Logger::Log("Created DirectionalLight shadow map: " + std::to_string(resolution) + "x" + std::to_string(resolution) + "\n");
}

void ShadowMapManager::CreateCascadeShadowMaps(uint32_t resolution) {
    cascadeShadowMap_.resolution = resolution;
    
    // 各カスケードの深度バッファ、DSV、SRVを作成
    for (uint32_t i = 0; i < ShadowMapConfig::kCascadeCount; ++i) {
        cascadeShadowMap_.depthBuffers[i] = CreateDepthBuffer(resolution, resolution);
        cascadeShadowMap_.dsvHandles[i] = CreateDSV(cascadeShadowMap_.depthBuffers[i].Get());
        cascadeShadowMap_.srvIndices[i] = CreateSRV(cascadeShadowMap_.depthBuffers[i].Get());
    }
    
    cascadeShadowMap_.isEnabled = true;
    
    KCE::Logger::Log("Created Cascade shadow maps: " + std::to_string(ShadowMapConfig::kCascadeCount) +
                " cascades, " + std::to_string(resolution) + "x" + std::to_string(resolution) + " each\n");
}

void ShadowMapManager::BeginCascadeShadowPass(uint32_t cascadeIndex) {
    if (!cascadeShadowMap_.isEnabled || cascadeIndex >= ShadowMapConfig::kCascadeCount) {
        return;
    }
    
    auto* commandList = dxCommon_->GetCommandList();
    
    // リソースバリア: GENERIC_READ -> DEPTH_WRITE
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = cascadeShadowMap_.depthBuffers[cascadeIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);
    
    // レンダーターゲットをクリアしてDSVのみを設定
    commandList->OMSetRenderTargets(0, nullptr, FALSE, &cascadeShadowMap_.dsvHandles[cascadeIndex]);
    
    // 深度バッファをクリア
    commandList->ClearDepthStencilView(
        cascadeShadowMap_.dsvHandles[cascadeIndex],
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f,
        0,
        0,
        nullptr
    );
    
    // ビューポートとシザー矩形の設定
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(cascadeShadowMap_.resolution);
    viewport.Height = static_cast<float>(cascadeShadowMap_.resolution);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    commandList->RSSetViewports(1, &viewport);
    
    D3D12_RECT scissorRect = {};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = static_cast<LONG>(cascadeShadowMap_.resolution);
    scissorRect.bottom = static_cast<LONG>(cascadeShadowMap_.resolution);
    commandList->RSSetScissorRects(1, &scissorRect);
    
    currentShadowMapResource_ = cascadeShadowMap_.depthBuffers[cascadeIndex].Get();
    currentCascadeIndex_ = cascadeIndex;
}

void ShadowMapManager::CreateSpotLightShadowMap(const std::string& name, uint32_t resolution) {
    // 既に存在する場合はスキップ
    if (spotLightShadowMaps_.find(name) != spotLightShadowMaps_.end()) {
        KCE::Logger::Log("SpotLight shadow map already exists: " + name + "\n");
        return;
    }
    
    ShadowMap shadowMap;
    shadowMap.depthBuffer = CreateDepthBuffer(resolution, resolution);
    shadowMap.resolution = resolution;
    shadowMap.type = ShadowMapType::Spot;
    shadowMap.dsvHandle = CreateDSV(shadowMap.depthBuffer.Get());
    shadowMap.srvIndex = CreateSRV(shadowMap.depthBuffer.Get());
    shadowMap.isEnabled = true;
    
    spotLightShadowMaps_.emplace(name, std::move(shadowMap));
    
    KCE::Logger::Log("Created SpotLight shadow map: " + name + " (" + std::to_string(resolution) + "x" + std::to_string(resolution) + ")\n");
}

void ShadowMapManager::CreatePointLightShadowMap(const std::string& name, uint32_t resolution) {
    // 既に存在する場合はスキップ
    if (pointLightShadowMaps_.find(name) != pointLightShadowMaps_.end()) {
        KCE::Logger::Log("PointLight shadow map already exists: " + name + "\n");
        return;
    }
    
    PointLightShadowMap shadowMap;
    shadowMap.depthBuffer = CreateDepthBuffer(resolution, resolution, 6); // キューブマップ用に6面
    shadowMap.resolution = resolution;
    
    // 各面のDSVを作成
    for (uint32_t i = 0; i < 6; ++i) {
        shadowMap.dsvHandles[i] = CreateDSV(shadowMap.depthBuffer.Get(), i);
    }
    
    // キューブマップSRVの作成
    shadowMap.srvIndex = CreateCubeSRV(shadowMap.depthBuffer.Get());
    shadowMap.isEnabled = true;
    
    pointLightShadowMaps_.emplace(name, std::move(shadowMap));
    
    KCE::Logger::Log("Created PointLight cube shadow map: " + name + " (" + std::to_string(resolution) + "x" + std::to_string(resolution) + ")\n");
}

void ShadowMapManager::BeginDirectionalLightShadowPass() {
    if (!directionalLightShadowMap_.isEnabled) {
        return;
    }
    
    auto* commandList = dxCommon_->GetCommandList();
    
    // リソースバリア: GENERIC_READ -> DEPTH_WRITE
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = directionalLightShadowMap_.depthBuffer.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);
    
    // レンダーターゲットをクリアしてDSVのみを設定
    commandList->OMSetRenderTargets(0, nullptr, FALSE, &directionalLightShadowMap_.dsvHandle);
    
    // 深度バッファをクリア
    commandList->ClearDepthStencilView(
        directionalLightShadowMap_.dsvHandle,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f,
        0,
        0,
        nullptr
    );
    
    // ビューポートとシザー矩形の設定
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(directionalLightShadowMap_.resolution);
    viewport.Height = static_cast<float>(directionalLightShadowMap_.resolution);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    commandList->RSSetViewports(1, &viewport);
    
    D3D12_RECT scissorRect = {};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = static_cast<LONG>(directionalLightShadowMap_.resolution);
    scissorRect.bottom = static_cast<LONG>(directionalLightShadowMap_.resolution);
    commandList->RSSetScissorRects(1, &scissorRect);
    
    currentShadowMapResource_ = directionalLightShadowMap_.depthBuffer.Get();
}

void ShadowMapManager::BeginSpotLightShadowPass(const std::string& name) {
    auto it = spotLightShadowMaps_.find(name);
    if (it == spotLightShadowMaps_.end() || !it->second.isEnabled) {
        return;
    }
    
    auto& shadowMap = it->second;
    auto* commandList = dxCommon_->GetCommandList();
    
    // リソースバリア: GENERIC_READ -> DEPTH_WRITE
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = shadowMap.depthBuffer.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);
    
    // レンダーターゲットをクリアしてDSVのみを設定
    commandList->OMSetRenderTargets(0, nullptr, FALSE, &shadowMap.dsvHandle);
    
    // 深度バッファをクリア
    commandList->ClearDepthStencilView(
        shadowMap.dsvHandle,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f,
        0,
        0,
        nullptr
    );
    
    // ビューポートとシザー矩形の設定
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(shadowMap.resolution);
    viewport.Height = static_cast<float>(shadowMap.resolution);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    commandList->RSSetViewports(1, &viewport);
    
    D3D12_RECT scissorRect = {};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = static_cast<LONG>(shadowMap.resolution);
    scissorRect.bottom = static_cast<LONG>(shadowMap.resolution);
    commandList->RSSetScissorRects(1, &scissorRect);
    
    currentShadowMapResource_ = shadowMap.depthBuffer.Get();
}

void ShadowMapManager::BeginPointLightShadowPass(const std::string& name, uint32_t faceIndex) {
    if (faceIndex >= 6) {
        return;
    }
    
    auto it = pointLightShadowMaps_.find(name);
    if (it == pointLightShadowMaps_.end() || !it->second.isEnabled) {
        return;
    }
    
    auto& shadowMap = it->second;
    auto* commandList = dxCommon_->GetCommandList();
    
    // 最初の面の時のみリソースバリアを設定
    if (faceIndex == 0) {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = shadowMap.depthBuffer.Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);
    }
    
    // レンダーターゲットをクリアしてDSVのみを設定
    commandList->OMSetRenderTargets(0, nullptr, FALSE, &shadowMap.dsvHandles[faceIndex]);
    
    // 深度バッファをクリア
    commandList->ClearDepthStencilView(
        shadowMap.dsvHandles[faceIndex],
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f,
        0,
        0,
        nullptr
    );
    
    // ビューポートとシザー矩形の設定
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(shadowMap.resolution);
    viewport.Height = static_cast<float>(shadowMap.resolution);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    commandList->RSSetViewports(1, &viewport);
    
    D3D12_RECT scissorRect = {};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = static_cast<LONG>(shadowMap.resolution);
    scissorRect.bottom = static_cast<LONG>(shadowMap.resolution);
    commandList->RSSetScissorRects(1, &scissorRect);
    
    currentShadowMapResource_ = shadowMap.depthBuffer.Get();
}

void ShadowMapManager::EndShadowPass() {
    if (!currentShadowMapResource_) {
        return;
    }
    
    auto* commandList = dxCommon_->GetCommandList();
    
    // リソースバリア: DEPTH_WRITE -> GENERIC_READ
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = currentShadowMapResource_;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);
    
    currentShadowMapResource_ = nullptr;
}

void ShadowMapManager::Clear() {
    spotLightShadowMaps_.clear();
    pointLightShadowMaps_.clear();
    directionalLightShadowMap_.isEnabled = false;
}

const ShadowMap& ShadowMapManager::GetSpotLightShadowMap(const std::string& name) const {
    auto it = spotLightShadowMaps_.find(name);
    if (it == spotLightShadowMaps_.end()) {
        KCE::Logger::Log("SpotLight shadow map not found: " + name + "\n");
        static ShadowMap empty;
        return empty;
    }
    return it->second;
}

ShadowMap& ShadowMapManager::GetSpotLightShadowMap(const std::string& name) {
    auto it = spotLightShadowMaps_.find(name);
    if (it == spotLightShadowMaps_.end()) {
        KCE::Logger::Log("SpotLight shadow map not found: " + name + "\n");
        static ShadowMap empty;
        return empty;
    }
    return it->second;
}

const PointLightShadowMap& ShadowMapManager::GetPointLightShadowMap(const std::string& name) const {
    auto it = pointLightShadowMaps_.find(name);
    if (it == pointLightShadowMaps_.end()) {
        KCE::Logger::Log("PointLight shadow map not found: " + name + "\n");
        static PointLightShadowMap empty;
        return empty;
    }
    return it->second;
}

PointLightShadowMap& ShadowMapManager::GetPointLightShadowMap(const std::string& name) {
    auto it = pointLightShadowMaps_.find(name);
    if (it == pointLightShadowMaps_.end()) {
        KCE::Logger::Log("PointLight shadow map not found: " + name + "\n");
        static PointLightShadowMap empty;
        return empty;
    }
    return it->second;
}

bool ShadowMapManager::HasSpotLightShadowMap(const std::string& name) const {
    return spotLightShadowMaps_.find(name) != spotLightShadowMaps_.end();
}

bool ShadowMapManager::HasPointLightShadowMap(const std::string& name) const {
    return pointLightShadowMaps_.find(name) != pointLightShadowMaps_.end();
}

Microsoft::WRL::ComPtr<ID3D12Resource> ShadowMapManager::CreateDepthBuffer(uint32_t width, uint32_t height, uint32_t arraySize) {
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.DepthOrArraySize = static_cast<UINT16>(arraySize);
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_R32_TYPELESS; // SRVでも使用するためTypeless
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    
    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
    
    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;
    
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        &clearValue,
        IID_PPV_ARGS(&resource)
    );
    assert(SUCCEEDED(hr));
    
    return resource;
}

D3D12_CPU_DESCRIPTOR_HANDLE ShadowMapManager::CreateDSV(ID3D12Resource* resource, uint32_t arraySlice) {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += dsvDescriptorSize_ * dsvIndex_;
    
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    
    D3D12_RESOURCE_DESC resDesc = resource->GetDesc();
    if (resDesc.DepthOrArraySize > 1) {
        // キューブマップの場合
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.FirstArraySlice = arraySlice;
        dsvDesc.Texture2DArray.ArraySize = 1;
        dsvDesc.Texture2DArray.MipSlice = 0;
    } else {
        // 通常の2Dテクスチャ
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = 0;
    }
    
    dxCommon_->GetDevice()->CreateDepthStencilView(resource, &dsvDesc, handle);
    
    dsvIndex_++;
    return handle;
}

uint32_t ShadowMapManager::CreateSRV(ID3D12Resource* resource) {
    uint32_t srvIndex = srvManager_->Allocate();
    
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT; // 深度値を読み取るためR32_FLOATを使用
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    
    dxCommon_->GetDevice()->CreateShaderResourceView(
        resource,
        &srvDesc,
        srvManager_->GetCPUDescriptorHandle(srvIndex)
    );
    
    return srvIndex;
}

uint32_t ShadowMapManager::CreateCubeSRV(ID3D12Resource* resource) {
    uint32_t srvIndex = srvManager_->Allocate();
    
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.TextureCube.MipLevels = 1;
    srvDesc.TextureCube.MostDetailedMip = 0;
    
    dxCommon_->GetDevice()->CreateShaderResourceView(
        resource,
        &srvDesc,
        srvManager_->GetCPUDescriptorHandle(srvIndex)
    );
    
    return srvIndex;
}
