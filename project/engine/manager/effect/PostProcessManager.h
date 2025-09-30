#pragma once
#include <memory>
#include <wrl.h>
#include <d3d12.h>
#include <string>

// effects
#include "effects/postprocess/BloomEffect.h"
#include "effects/postprocess/CRTEffect.h"
#include "effects/postprocess/GrayscaleEffect.h"
#include "effects/postprocess/NoiseEffect.h"
#include "effects/postprocess/VignetteEffect.h"

class DirectXCommon;
class SrvManager;
class RenderTexture;

class PostProcessManager
{
public:
    PostProcessManager();
    ~PostProcessManager();

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const std::wstring& vsPath, const std::wstring& psPath);
    void Draw(RenderTexture* inputTexture);

    void RenderBrightPass(RenderTexture* inputTexture, RenderTexture* outputRT);
    void RenderBlurPass(RenderTexture* inputTexture, RenderTexture* outputRT, bool horizontal);
    void RenderFinalComposite(RenderTexture* sceneTexture, RenderTexture* bloomTexture);

    void SetBloomRenderTargets(RenderTexture* brightPassRT, RenderTexture* blurRT0, RenderTexture* blurRT1);

    std::unique_ptr<GrayscaleEffect> grayscaleEffect_;
    std::unique_ptr<VignetteEffect> vignetteEffect_;
    std::unique_ptr<NoiseEffect> noiseEffect_;
    std::unique_ptr<CRTEffect> crtEffect_;
    std::unique_ptr<BloomEffect> bloomEffect_;

	struct BrightPassParams
    {
        float threshold = 0.8f;
        float intensity = 2.0f;
        float knee = 0.5f;
        float padding = 0.0f;
    } brightPassParams_;

    struct BlurParams
    {
        Vector2 texelSize;
        Vector2 blurDirection;
        float radius = 8.0f;
        float padding[3] = {};
    } blurParams_;

private:
    void CreateConstantBuffer();
    void UpdateConstantBuffer();
    void SetupPipeline(const std::wstring& vsPath, const std::wstring& psPath);
    void CreateBloomPipelines();
    void RenderSinglePass(RenderTexture* inputTexture);
    void RenderWithBloom(RenderTexture* inputTexture);
    bool HasBloomRenderTargets() const;

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    // ビューポートとシザー矩形の設定
    D3D12_VIEWPORT viewport_ = {};
	D3D12_RECT scissorRect_ = {};

    // パイプライン
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> singlePassRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> singlePassPSO_;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> brightPassPSO_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> bloomRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> blurPSO_;
    Microsoft::WRL::ComPtr<ID3D12Resource> brightPassConstantBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> blurConstantBuffer_;

    RenderTexture* brightPassRT_ = nullptr;
    RenderTexture* blurRT_[2] = { nullptr, nullptr };

    PostEffectParams params_;
    PostEffectParams preParams_;

    
};