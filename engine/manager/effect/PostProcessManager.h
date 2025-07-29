#pragma once
#include <memory>
#include <wrl.h>
#include <d3d12.h>
#include <string>

// effects
#include "effects/postprocess/CRTEffect.h"
#include "effects/postprocess/GrayscaleEffect.h"
#include "effects/postprocess/NoiseEffect.h"
#include "effects/postprocess/VignetteEffect.h"

class DirectXCommon;
class SrvManager;

class PostProcessManager
{
public:
    PostProcessManager();
    ~PostProcessManager();

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const std::wstring& vsPath, const std::wstring& psPath);
    void Draw(D3D12_GPU_DESCRIPTOR_HANDLE inputTexture);

	std::unique_ptr<GrayscaleEffect> grayscaleEffect_;
	std::unique_ptr<VignetteEffect> vignetteEffect_;
	std::unique_ptr<NoiseEffect> noiseEffect_;
	std::unique_ptr<CRTEffect> crtEffect_;

private:
	void CreateConstantBuffer();
	void UpdateConstantBuffer();

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    //定数バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;

    void SetupPipeline(const std::wstring& vsPath, const std::wstring& psPath);
	PostEffectParams params_;
	PostEffectParams preParams_; // 前フレームのパラメータを保持
};
