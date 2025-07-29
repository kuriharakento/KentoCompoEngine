#include "PostProcessManager.h"

#include "DirectXTex/d3dx12.h"
#include <cassert>
// system
#include "engine/base/DirectXCommon.h"
#include "manager/system/SrvManager.h"

PostProcessManager::PostProcessManager() {}

PostProcessManager::~PostProcessManager() {}

void PostProcessManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const std::wstring& vsPath, const std::wstring& psPath)
{
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	SetupPipeline(vsPath, psPath);

	CreateConstantBuffer();

	grayscaleEffect_ = std::make_unique<GrayscaleEffect>();
	vignetteEffect_ = std::make_unique<VignetteEffect>();
	noiseEffect_ = std::make_unique<NoiseEffect>();
	crtEffect_ = std::make_unique<CRTEffect>();

	preParams_ = {};
}

void PostProcessManager::SetupPipeline(const std::wstring& vsPath, const std::wstring& psPath)
{
	// ルートシグネチャの作成
	CD3DX12_DESCRIPTOR_RANGE srvRange{};
	srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE samplerRange{};
	samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

	CD3DX12_ROOT_PARAMETER rootParams[3]{};
	rootParams[0].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[1].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);
	//　ポストプロセスのエフェクト用の定数バッファ
	rootParams[2].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc{};
	rootSigDesc.Init(_countof(rootParams), rootParams, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	Microsoft::WRL::ComPtr<ID3DBlob> sigBlob, errBlob;
	D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
	dxCommon_->GetDevice()->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));

	// パイプラインステートの作成
	auto vs = dxCommon_->CompileSharder(vsPath, L"vs_6_0");
	auto ps = dxCommon_->CompileSharder(psPath, L"ps_6_0");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	psoDesc.pRootSignature = rootSignature_.Get();
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.NumRenderTargets = 1;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.InputLayout = { nullptr, 0 };
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

	dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
}

void PostProcessManager::Draw(D3D12_GPU_DESCRIPTOR_HANDLE inputTexture)
{
	auto cmdList = dxCommon_->GetCommandList();

	cmdList->SetPipelineState(pipelineState_.Get());
	cmdList->SetGraphicsRootSignature(rootSignature_.Get());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// SRVヒープとSamplerヒープを両方指定
	ID3D12DescriptorHeap* heaps[] = {
		srvManager_->GetSrvHeap(),
		dxCommon_->GetSamplerHeap()
	};
	cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
	cmdList->SetGraphicsRootDescriptorTable(0, inputTexture);
	cmdList->SetGraphicsRootDescriptorTable(1, dxCommon_->GetSamplerDescriptorHandle());

	// ポストプロセスのエフェクトパラメータを設定
	grayscaleEffect_->ApplyEffect(params_);
	vignetteEffect_->ApplyEffect(params_);
	noiseEffect_->ApplyEffect(params_);
	crtEffect_->ApplyEffect(params_);
	// 定数バッファを更新
	UpdateConstantBuffer();
	// 定数バッファビューを設定
	cmdList->SetGraphicsRootConstantBufferView(2, constantBuffer_->GetGPUVirtualAddress());
	// 描画
	cmdList->DrawInstanced(3, 1, 0, 0);
}

void PostProcessManager::CreateConstantBuffer()
{
	size_t bufferSize = (sizeof(PostEffectParams) + 255) & ~255; // 256バイトアラインメント

	D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constantBuffer_));
	assert(SUCCEEDED(hr));

	// サンプラーヒープの作成
	dxCommon_->CreateSamplerHeap();
}

void PostProcessManager::UpdateConstantBuffer()
{
	if (params_ == preParams_)
	{
		return; // 前フレームと同じパラメータなら更新しない
	}

	void* mappedData = nullptr;
	constantBuffer_->Map(0, nullptr, &mappedData);
	memcpy(mappedData, &params_, sizeof(PostEffectParams));
	constantBuffer_->Unmap(0, nullptr);
	preParams_ = params_; // 前フレームのパラメータを更新
}
