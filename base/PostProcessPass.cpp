#include "PostProcessPass.h"

#include <cassert>
#include "DirectXTex/d3dx12.h"
// system
#include "engine/base/DirectXCommon.h"
#include "manager/system/SrvManager.h"

// 256バイトアラインメント用マスク
constexpr size_t kAlignmentMask = 255;
// SRV用のディスクリプタレンジ数
constexpr UINT kSrvRangeCount = 1;
// サンプラー用のディスクリプタレンジ数
constexpr UINT kSamplerRangeCount = 1;
// ルートパラメータ数
constexpr UINT kRootParameterCount = 3;
// レンダーターゲット数
constexpr UINT kNumRenderTargets = 1;
// 頂点数（フルスクリーン三角形）
constexpr UINT kVertexCount = 3;
// インスタンス数
constexpr UINT kInstanceCount = 1;
// ディスクリプタヒープ数
constexpr UINT kDescriptorHeapCount = 2;



void PostProcessPass::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const std::wstring& vsPath, const std::wstring& psPath)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    // SRVディスクリプタレンジの設定
    CD3DX12_DESCRIPTOR_RANGE range{};
    range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, kSrvRangeCount, 0);

    // サンプラーディスクリプタレンジの設定
    CD3DX12_DESCRIPTOR_RANGE samplerRange{};
    samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, kSamplerRangeCount, 0);

    // ルートパラメータの設定
    CD3DX12_ROOT_PARAMETER rootParams[kRootParameterCount]{};
    rootParams[0].InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParams[1].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParams[2].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

    // ルートシグネチャの設定
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc{};
    rootSigDesc.Init(_countof(rootParams), rootParams, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // ルートシグネチャのシリアライズと生成
    Microsoft::WRL::ComPtr<ID3DBlob> sigBlob, errBlob;
    D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
    dxCommon_->GetDevice()->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));

    // シェーダーのコンパイル
    auto vs = dxCommon_->CompileSharder(vsPath, L"vs_6_0");
    auto ps = dxCommon_->CompileSharder(psPath, L"ps_6_0");

    // パイプラインステートの設定
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.NumRenderTargets = kNumRenderTargets;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.InputLayout = { nullptr, 0 };
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

    // パイプラインステートの生成
    dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));

    // 定数バッファの作成
    D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    // 256バイトアラインメント
    D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(EffectSettings) + kAlignmentMask) & ~kAlignmentMask);

    HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&constantBuffer_));
    assert(SUCCEEDED(hr));

    // 定数バッファの初期化
    UpdateConstantBuffer();

	// Samplerヒープの作成
    dxCommon_->CreateSamplerHeap();
}

void PostProcessPass::Draw(D3D12_GPU_DESCRIPTOR_HANDLE srvHandle)
{
    auto cmdList = dxCommon_->GetCommandList();

    // パイプラインステートとルートシグネチャを設定
    cmdList->SetPipelineState(pipelineState_.Get());
    cmdList->SetGraphicsRootSignature(rootSignature_.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // SRVヒープとSamplerヒープを両方指定
    ID3D12DescriptorHeap* heaps[] = {
        srvManager_->GetSrvHeap(),
        dxCommon_->GetSamplerHeap()
    };
    cmdList->SetDescriptorHeaps(kDescriptorHeapCount, heaps);

    // ディスクリプタテーブルと定数バッファを設定
    cmdList->SetGraphicsRootDescriptorTable(0, srvHandle);
    cmdList->SetGraphicsRootDescriptorTable(1, dxCommon_->GetSamplerDescriptorHandle());
    cmdList->SetGraphicsRootConstantBufferView(2, constantBuffer_->GetGPUVirtualAddress());

    // フルスクリーン三角形を描画
    cmdList->DrawInstanced(kVertexCount, kInstanceCount, 0, 0);
}

void PostProcessPass::SetGrayscale(bool use)
{
	effectSettings_.useGrayscale = use;
	UpdateConstantBuffer();
}

void PostProcessPass::UpdateConstantBuffer()
{
    // 定数バッファにエフェクト設定をコピー
    void* mappedData = nullptr;
    constantBuffer_->Map(0, nullptr, &mappedData);
    memcpy(mappedData, &effectSettings_, sizeof(effectSettings_));
    constantBuffer_->Unmap(0, nullptr);
}
