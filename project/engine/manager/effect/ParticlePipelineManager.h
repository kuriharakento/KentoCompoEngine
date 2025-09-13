#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <unordered_map>

#include "math/BlendMode.h"

class DirectXCommon;

class ParticlePipelineManager
{
public:
    ParticlePipelineManager() = default;
    ~ParticlePipelineManager() = default;

    void Initialize(DirectXCommon* dxCommon);

	ID3D12PipelineState* GetPipelineState(BlendMode mode) const;
    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }

private:
    void CreateRootSignature();
	void CreateGraphicsPipelineState(BlendMode mode);

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    std::unordered_map<BlendMode, Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelines_;
	DirectXCommon* dxCommon_ = nullptr;
};
