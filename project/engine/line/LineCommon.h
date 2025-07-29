#pragma once
#include <wrl.h>
#include <d3d12.h>

class DirectXCommon;

class LineCommon {
public:
    void Initialize(DirectXCommon* dxCommon);
    Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineState() const { return pipelineState_; }
    Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature() const { return rootSignature_; }
    DirectXCommon* GetDirectXCommon() const { return dxCommon_; }

private:
    void CreateGraphicsPipelineState();
    void CreateRootSignature();

    DirectXCommon* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
};
