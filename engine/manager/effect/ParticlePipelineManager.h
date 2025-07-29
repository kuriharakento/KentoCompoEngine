#pragma once
#include <wrl.h>
#include <d3d12.h>

class DirectXCommon;

class ParticlePipelineManager
{
public:
    ParticlePipelineManager() = default;
    ~ParticlePipelineManager() = default;

    void Initialize(DirectXCommon* dxCommon);

    ID3D12PipelineState* GetPipelineState() const { return graphicsPipelineState_.Get(); }
    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
	DirectXCommon* dxCommon_ = nullptr;
};
