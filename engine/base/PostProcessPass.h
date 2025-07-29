#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <string>

class DirectXCommon;
class SrvManager;

class PostProcessPass
{
public:
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const std::wstring& vsPath, const std::wstring& psPath);
    void Draw(D3D12_GPU_DESCRIPTOR_HANDLE srvHandle);

    void SetGrayscale(bool use);

private:
	void UpdateConstantBuffer();

private:
    struct EffectSettings
    {
        bool useGrayscale = false;
    } effectSettings_;
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;

 
};

