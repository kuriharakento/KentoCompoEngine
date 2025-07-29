#pragma once  

#include <wrl.h>
#include <d3d12.h>  
#include <cstdint>
#include "math/Vector4.h"

class DirectXCommon;
class SrvManager;

class RenderTexture  
{  
public:
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor);
    void BeginRender();
    void EndRender();
	void PreDrawForImGui();
	void PostDrawForImGui();


public: // ゲッター

    ID3D12Resource* GetResource() const { return texture_.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle() const;
    uint32_t GetSRVIndex() const { return srvIndex_; }

private:

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> texture_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};
    uint32_t srvIndex_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    Vector4 clearColor_ = { 0, 0, 0, 1 };
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    D3D12_RESOURCE_STATES currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;

};
