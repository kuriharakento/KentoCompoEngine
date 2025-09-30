#include "PostProcessManager.h"

#include "DirectXTex/d3dx12.h"
#include <cassert>
// system
#include "engine/base/DirectXCommon.h"
#include "manager/system/SrvManager.h"
#include "base/RenderTexture.h"

PostProcessManager::PostProcessManager() {}

PostProcessManager::~PostProcessManager() {}

void PostProcessManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const std::wstring& vsPath, const std::wstring& psPath)
{
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	// パイプライン作成
	SetupPipeline(vsPath, psPath);
	// Bloom用のパイプライン作成
	CreateBloomPipelines();
	// 定数バッファの作成
	CreateConstantBuffer();

	// エフェクトの初期化
	grayscaleEffect_ = std::make_unique<GrayscaleEffect>();
	vignetteEffect_ = std::make_unique<VignetteEffect>();
	noiseEffect_ = std::make_unique<NoiseEffect>();
	crtEffect_ = std::make_unique<CRTEffect>();
	bloomEffect_ = std::make_unique<BloomEffect>();

	preParams_ = {};

	// ビューポートとシザー矩形の設定
	viewport_.Width = WinApp::kClientWidth;
	viewport_.Height = WinApp::kClientHeight;
	viewport_.TopLeftX = 0;
	viewport_.TopLeftY = 0;
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;

	scissorRect_.left = 0;
	scissorRect_.right = WinApp::kClientWidth;
	scissorRect_.top = 0;
	scissorRect_.bottom = WinApp::kClientHeight;
}

void PostProcessManager::SetupPipeline(const std::wstring& vsPath, const std::wstring& psPath)
{
	// ルートシグネチャの作成
	CD3DX12_DESCRIPTOR_RANGE sceneTextureRange{};
	sceneTextureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0

	CD3DX12_DESCRIPTOR_RANGE bloomTextureRange{};
	bloomTextureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // t1

	CD3DX12_DESCRIPTOR_RANGE samplerRange{};
	samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

	CD3DX12_ROOT_PARAMETER rootParams[4]{};
	rootParams[0].InitAsDescriptorTable(1, &sceneTextureRange, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[1].InitAsDescriptorTable(1, &bloomTextureRange, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[2].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[3].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

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

void PostProcessManager::CreateBloomPipelines()
{
	// ルートシグネチャの作成
	CD3DX12_DESCRIPTOR_RANGE sceneTextureRange{};
	sceneTextureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0

	CD3DX12_DESCRIPTOR_RANGE samplerRange{};
	samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

	CD3DX12_ROOT_PARAMETER rootParams[3]{};
	rootParams[0].InitAsDescriptorTable(1, &sceneTextureRange, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[1].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[2].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL); // b0

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc{};
	rootSigDesc.Init(_countof(rootParams), rootParams, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	Microsoft::WRL::ComPtr<ID3DBlob> sigBlob, errBlob;
	D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
	dxCommon_->GetDevice()->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&bloomRootSignature_));


	// BrightPassパイプライン作成
	{
		auto vs = dxCommon_->CompileSharder(L"Resources/shaders/PostEffect.VS.hlsl", L"vs_6_0");
		auto ps = dxCommon_->CompileSharder(L"Resources/shaders/BrightPass.PS.hlsl", L"ps_6_0");

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		psoDesc.pRootSignature = bloomRootSignature_.Get();
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;  // HDRフォーマット
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

		dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&brightPassPSO_));
	}

	// BlurPassパイプライン作成
	{
		auto vs = dxCommon_->CompileSharder(L"Resources/shaders/PostEffect.VS.hlsl", L"vs_6_0");
		auto ps = dxCommon_->CompileSharder(L"Resources/shaders/GaussianBlur.PS.hlsl", L"ps_6_0");

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		psoDesc.pRootSignature = bloomRootSignature_.Get();
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
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

		dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&blurPSO_));
	}

	// 定数バッファ作成
	size_t brightPassBufferSize = (sizeof(BrightPassParams) + 255) & ~255;
	size_t blurBufferSize = (sizeof(BlurParams) + 255) & ~255;

	D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	D3D12_RESOURCE_DESC brightPassBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(brightPassBufferSize);
	dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &brightPassBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&brightPassConstantBuffer_));

	D3D12_RESOURCE_DESC blurBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(blurBufferSize);
	dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &blurBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&blurConstantBuffer_));
}

void PostProcessManager::RenderBrightPass(RenderTexture* inputTexture, RenderTexture* outputRT)
{
	auto cmdList = dxCommon_->GetCommandList();

	// レンダーターゲット設定
	outputRT->BeginRender();

	// パイプライン設定
	cmdList->SetPipelineState(brightPassPSO_.Get());
	cmdList->SetGraphicsRootSignature(bloomRootSignature_.Get());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ヒープ設定
	ID3D12DescriptorHeap* heaps[] = {
		srvManager_->GetSrvHeap(),
		dxCommon_->GetSamplerHeap()
	};
	cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
	cmdList->SetGraphicsRootDescriptorTable(0, inputTexture->GetGPUHandle());
	cmdList->SetGraphicsRootDescriptorTable(1, dxCommon_->GetSamplerDescriptorHandle());

	// 定数バッファ更新
	void* mappedData = nullptr;
	brightPassConstantBuffer_->Map(0, nullptr, &mappedData);
	memcpy(mappedData, &brightPassParams_, sizeof(BrightPassParams));
	brightPassConstantBuffer_->Unmap(0, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(2, brightPassConstantBuffer_->GetGPUVirtualAddress());

	// 描画
	cmdList->DrawInstanced(3, 1, 0, 0);

	outputRT->EndRender();
}

void PostProcessManager::RenderBlurPass(RenderTexture* inputTexture, RenderTexture* outputRT, bool horizontal)
{
	auto cmdList = dxCommon_->GetCommandList();

	outputRT->BeginRender();

	cmdList->SetPipelineState(blurPSO_.Get());
	cmdList->SetGraphicsRootSignature(bloomRootSignature_.Get());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D12DescriptorHeap* heaps[] = {
		srvManager_->GetSrvHeap(),
		dxCommon_->GetSamplerHeap()
	};
	cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
	cmdList->SetGraphicsRootDescriptorTable(0, inputTexture->GetGPUHandle());
	cmdList->SetGraphicsRootDescriptorTable(1, dxCommon_->GetSamplerDescriptorHandle());

	// ブラー方向設定
	blurParams_.texelSize = { 1.0f / WinApp::kClientWidth, 1.0f / WinApp::kClientHeight };
	blurParams_.blurDirection = horizontal ? Vector2{ 1.0f, 0.0f } : Vector2{ 0.0f, 1.0f };

	void* mappedData = nullptr;
	blurConstantBuffer_->Map(0, nullptr, &mappedData);
	memcpy(mappedData, &blurParams_, sizeof(BlurParams));
	blurConstantBuffer_->Unmap(0, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(2, blurConstantBuffer_->GetGPUVirtualAddress());

	cmdList->DrawInstanced(3, 1, 0, 0);

	outputRT->EndRender();
}

void PostProcessManager::RenderFinalComposite(RenderTexture* sceneTexture, RenderTexture* bloomTexture)
{
	auto cmdList = dxCommon_->GetCommandList();
	auto device = dxCommon_->GetDevice();

	UINT backBufferIndex = dxCommon_->GetCurrentBackBufferIndex();
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dxCommon_->GetCPUDescriptorHandle(dxCommon_->GetRTVDescriptorHeap(), dxCommon_->GetDescriptorSizeRTV(), backBufferIndex);
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVDescriptorHeap()->GetCPUDescriptorHandleForHeapStart();
	cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// ビューポートとシザー矩形の設定
	cmdList->RSSetViewports(1, &viewport_);
	cmdList->RSSetScissorRects(1, &scissorRect_);

	cmdList->SetPipelineState(pipelineState_.Get());
	cmdList->SetGraphicsRootSignature(rootSignature_.Get());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D12DescriptorHeap* heaps[] = {
		srvManager_->GetSrvHeap(),
		dxCommon_->GetSamplerHeap()
	};
	cmdList->SetDescriptorHeaps(_countof(heaps), heaps);

	// 2つのテクスチャを別々のディスクリプタテーブルで設定
	cmdList->SetGraphicsRootDescriptorTable(0, sceneTexture->GetGPUHandle());  // t0
	cmdList->SetGraphicsRootDescriptorTable(1, bloomTexture->GetGPUHandle());  // t1
	cmdList->SetGraphicsRootDescriptorTable(2, dxCommon_->GetSamplerDescriptorHandle());

	// 既存のエフェクト適用
	grayscaleEffect_->ApplyEffect(params_);
	vignetteEffect_->ApplyEffect(params_);
	noiseEffect_->ApplyEffect(params_);
	crtEffect_->ApplyEffect(params_);
	bloomEffect_->ApplyEffect(params_);

	UpdateConstantBuffer();
	cmdList->SetGraphicsRootConstantBufferView(3, constantBuffer_->GetGPUVirtualAddress());

	cmdList->DrawInstanced(3, 1, 0, 0);
}

void PostProcessManager::SetBloomRenderTargets(RenderTexture* brightPassRT, RenderTexture* blurRT0, RenderTexture* blurRT1)
{
	brightPassRT_ = brightPassRT;
	blurRT_[0] = blurRT0;
	blurRT_[1] = blurRT1;
}

void PostProcessManager::Draw(RenderTexture* inputTexture)
{
	// ブルームが有効かつ、必要なレンダーターゲットが存在する場合
	if (bloomEffect_->IsEnabled() && HasBloomRenderTargets())
	{
		// マルチパス・ブルーム処理
		RenderWithBloom(inputTexture);
	}
	else
	{
		// 従来のシングルパス処理
		RenderSinglePass(inputTexture);
	}
}

void PostProcessManager::RenderWithBloom(RenderTexture* inputTexture)
{
	// 1. ブライトパス（明るい部分の抽出）
	RenderBrightPass(inputTexture, brightPassRT_);

	// 2. 水平ブラー
	RenderBlurPass(brightPassRT_, blurRT_[0], true);

	// 3. 垂直ブラー
	RenderBlurPass(blurRT_[0], blurRT_[1], false);

	// 4. 最終合成（シーン + ブルーム）
	RenderFinalComposite(inputTexture, blurRT_[1]);
}

bool PostProcessManager::HasBloomRenderTargets() const
{
	return brightPassRT_ != nullptr && blurRT_[0] != nullptr && blurRT_[1] != nullptr;
}

void PostProcessManager::RenderSinglePass(RenderTexture* inputTexture)
{
	auto cmdList = dxCommon_->GetCommandList();

	UINT backBufferIndex = dxCommon_->GetCurrentBackBufferIndex();
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dxCommon_->GetCPUDescriptorHandle(dxCommon_->GetRTVDescriptorHeap(), dxCommon_->GetDescriptorSizeRTV(), backBufferIndex);
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVDescriptorHeap()->GetCPUDescriptorHandleForHeapStart();
	cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// ビューポートとシザー矩形の設定
	cmdList->RSSetViewports(1, &viewport_);
	cmdList->RSSetScissorRects(1, &scissorRect_);

	cmdList->SetPipelineState(pipelineState_.Get());
	cmdList->SetGraphicsRootSignature(rootSignature_.Get());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// SRVヒープとSamplerヒープを両方指定
	ID3D12DescriptorHeap* heaps[] = {
		srvManager_->GetSrvHeap(),
		dxCommon_->GetSamplerHeap()
	};
	cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
	cmdList->SetGraphicsRootDescriptorTable(0, inputTexture->GetGPUHandle());
	cmdList->SetGraphicsRootDescriptorTable(1, blurRT_[1]->GetGPUHandle()); // ダミーのブルームテクスチャ
	cmdList->SetGraphicsRootDescriptorTable(2, dxCommon_->GetSamplerDescriptorHandle());

	// ポストプロセスのエフェクトパラメータを設定
	grayscaleEffect_->ApplyEffect(params_);
	vignetteEffect_->ApplyEffect(params_);
	noiseEffect_->ApplyEffect(params_);
	crtEffect_->ApplyEffect(params_);
	bloomEffect_->ApplyEffect(params_);

	// 定数バッファを更新
	UpdateConstantBuffer();
	// 定数バッファビューを設定
	cmdList->SetGraphicsRootConstantBufferView(3, constantBuffer_->GetGPUVirtualAddress());
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