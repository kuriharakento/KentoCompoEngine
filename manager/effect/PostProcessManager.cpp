#include "graphics/RenderFormats.h"
#include "PostProcessManager.h"

#include "DirectXTex/d3dx12.h"
#include <cassert>
// system
#include "engine/base/DirectXCommon.h"
#include "manager/system/SrvManager.h"
#include "base/RenderTexture.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "manager/editor/DebugUIManager.h"
#endif

namespace KCE
{
PostProcessManager::PostProcessManager() {}

PostProcessManager::~PostProcessManager() {}

void PostProcessManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const std::wstring& vsPath, const std::wstring& psPath, uint32_t width, uint32_t height)
{
	// 引数をメンバ変数に記録
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	// ホットリロードで再コンパイルするために保持しておく
	vsPath_ = vsPath;
	psPath_ = psPath;

	// パイプライン作成
	SetupPipeline(vsPath, psPath);
	// Bloom用のパイプライン作成
	CreateBloomPipelines();
	// 定数バッファの作成
	CreateConstantBuffer();

	// 各エフェクトの初期化
	grayscaleEffect_ = std::make_unique<GrayscaleEffect>();
	vignetteEffect_ = std::make_unique<VignetteEffect>();
	noiseEffect_ = std::make_unique<NoiseEffect>();
	crtEffect_ = std::make_unique<CRTEffect>();
	bloomEffect_ = std::make_unique<BloomEffect>();
	tonemapEffect_ = std::make_unique<TonemapEffect>();

	// ブルームの初期テクセルサイズを設定
	bloomEffect_->SetInvScreenSize({ 1.0f / width, 1.0f / height });

	// 前フレームパラメータを初期化
	preParams_ = {};

	// ビューポートの設定
	viewport_.Width = static_cast<float>(width);
	viewport_.Height = static_cast<float>(height);
	viewport_.TopLeftX = 0;
	viewport_.TopLeftY = 0;
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;

	// シザー矩形の設定
	scissorRect_.left = 0;
	scissorRect_.right = static_cast<LONG>(width);
	scissorRect_.top = 0;
	scissorRect_.bottom = static_cast<LONG>(height);
}

void PostProcessManager::SetupPipeline(const std::wstring& vsPath, const std::wstring& psPath)
{
	/*--------------[ ルートシグネチャの作成 ]-----------------*/

	// シーンテクスチャ用ディスクリプタレンジ（t0）
	CD3DX12_DESCRIPTOR_RANGE sceneTextureRange{};
	sceneTextureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	// ブルームテクスチャ用ディスクリプタレンジ（t1）
	CD3DX12_DESCRIPTOR_RANGE bloomTextureRange{};
	bloomTextureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

	// サンプラー用ディスクリプタレンジ
	CD3DX12_DESCRIPTOR_RANGE samplerRange{};
	samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

	// ルートパラメータの設定
	CD3DX12_ROOT_PARAMETER rootParams[4]{};
	rootParams[0].InitAsDescriptorTable(1, &sceneTextureRange, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[1].InitAsDescriptorTable(1, &bloomTextureRange, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[2].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[3].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

	// ルートシグネチャの記述
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc{};
	rootSigDesc.Init(_countof(rootParams), rootParams, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// ルートシグネチャのシリアライズと生成
	Microsoft::WRL::ComPtr<ID3DBlob> sigBlob, errBlob;
	D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
	dxCommon_->GetDevice()->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));

	/*--------------[ パイプラインステートの作成 ]-----------------*/

	// シェーダーのコンパイル
	auto vs = dxCommon_->CompileSharder(vsPath, L"vs_6_0");
	auto ps = dxCommon_->CompileSharder(psPath, L"ps_6_0");

	// パイプラインステートの記述
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	psoDesc.pRootSignature = rootSignature_.Get();
	psoDesc.RTVFormats[0] = kDisplayColorFormat;
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

	// パイプラインステートの生成
	dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
}

void PostProcessManager::CreateBloomPipelines()
{
	/*--------------[ ルートシグネチャの作成 ]-----------------*/

	// シーンテクスチャ用ディスクリプタレンジ（t0）
	CD3DX12_DESCRIPTOR_RANGE sceneTextureRange{};
	sceneTextureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	// サンプラー用ディスクリプタレンジ
	CD3DX12_DESCRIPTOR_RANGE samplerRange{};
	samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

	// ルートパラメータの設定
	CD3DX12_ROOT_PARAMETER rootParams[3]{};
	rootParams[0].InitAsDescriptorTable(1, &sceneTextureRange, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[1].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[2].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

	// ルートシグネチャの記述
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc{};
	rootSigDesc.Init(_countof(rootParams), rootParams, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// ルートシグネチャのシリアライズと生成
	Microsoft::WRL::ComPtr<ID3DBlob> sigBlob, errBlob;
	D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
	dxCommon_->GetDevice()->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&bloomRootSignature_));


	/*--------------[ ブライトパスパイプラインの作成 ]-----------------*/
	{
		// シェーダーのコンパイル
		auto vs = dxCommon_->CompileSharder(L"Resources/shaders/PostEffect.VS.hlsl", L"vs_6_0");
		auto ps = dxCommon_->CompileSharder(L"Resources/shaders/BrightPass.PS.hlsl", L"ps_6_0");

		// パイプラインステートの記述
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		psoDesc.pRootSignature = bloomRootSignature_.Get();
		psoDesc.RTVFormats[0] = kBloomBufferFormat;
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

		// パイプラインステートの生成
		dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&brightPassPSO_));
	}

	/*--------------[ ブラーパスパイプラインの作成 ]-----------------*/
	{
		// シェーダーのコンパイル
		auto vs = dxCommon_->CompileSharder(L"Resources/shaders/PostEffect.VS.hlsl", L"vs_6_0");
		auto ps = dxCommon_->CompileSharder(L"Resources/shaders/GaussianBlur.PS.hlsl", L"ps_6_0");

		// パイプラインステートの記述
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		psoDesc.pRootSignature = bloomRootSignature_.Get();
		psoDesc.RTVFormats[0] = kBloomBufferFormat;
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

		// パイプラインステートの生成
		dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&blurPSO_));
	}

	/*--------------[ 定数バッファの作成 ]-----------------*/

	// バッファサイズを256バイトアラインメント
	size_t brightPassBufferSize = (sizeof(BrightPassParams) + 255) & ~255;
	size_t blurBufferSize = (sizeof(BlurParams) + 255) & ~255;

	// ヒーププロパティの設定
	D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	// ブライトパス用定数バッファの作成
	D3D12_RESOURCE_DESC brightPassBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(brightPassBufferSize);
	dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &brightPassBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&brightPassConstantBuffer_));

	// ブラー用定数バッファの作成
	D3D12_RESOURCE_DESC blurBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(blurBufferSize);
	dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &blurBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&blurConstantBuffer_));
}

void PostProcessManager::RenderBrightPass(RenderTexture* inputTexture, RenderTexture* outputRT)
{
	auto cmdList = dxCommon_->GetCommandList();

	// 出力レンダーターゲットを設定
	outputRT->BeginRender();

	// パイプラインステートとルートシグネチャを設定
	cmdList->SetPipelineState(brightPassPSO_.Get());
	cmdList->SetGraphicsRootSignature(bloomRootSignature_.Get());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ディスクリプタヒープを設定
	ID3D12DescriptorHeap* heaps[] = {
		srvManager_->GetSrvHeap(),
		dxCommon_->GetSamplerHeap()
	};
	cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
	cmdList->SetGraphicsRootDescriptorTable(0, inputTexture->GetGPUHandle());
	cmdList->SetGraphicsRootDescriptorTable(1, dxCommon_->GetSamplerDescriptorHandle());

	// 定数バッファを更新してGPUに転送
	void* mappedData = nullptr;
	brightPassConstantBuffer_->Map(0, nullptr, &mappedData);
	memcpy(mappedData, &brightPassParams_, sizeof(BrightPassParams));
	brightPassConstantBuffer_->Unmap(0, nullptr);

	// 定数バッファビューを設定
	cmdList->SetGraphicsRootConstantBufferView(2, brightPassConstantBuffer_->GetGPUVirtualAddress());

	// フルスクリーン三角形を描画
	cmdList->DrawInstanced(3, 1, 0, 0);

	// レンダーターゲットを終了
	outputRT->EndRender();
}

void PostProcessManager::RenderBlurPass(RenderTexture* inputTexture, RenderTexture* outputRT, bool horizontal)
{
	auto cmdList = dxCommon_->GetCommandList();

	// 出力レンダーターゲットを設定
	outputRT->BeginRender();

	// パイプラインステートとルートシグネチャを設定
	cmdList->SetPipelineState(blurPSO_.Get());
	cmdList->SetGraphicsRootSignature(bloomRootSignature_.Get());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ディスクリプタヒープを設定
	ID3D12DescriptorHeap* heaps[] = {
		srvManager_->GetSrvHeap(),
		dxCommon_->GetSamplerHeap()
	};
	cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
	cmdList->SetGraphicsRootDescriptorTable(0, inputTexture->GetGPUHandle());
	cmdList->SetGraphicsRootDescriptorTable(1, dxCommon_->GetSamplerDescriptorHandle());

	// ブラー方向を設定（水平または垂直）
	blurParams_.texelSize = { 1.0f / viewport_.Width, 1.0f / viewport_.Height };
	blurParams_.blurDirection = horizontal ? Vector2{ 1.0f, 0.0f } : Vector2{ 0.0f, 1.0f };

	// 定数バッファを更新してGPUに転送
	void* mappedData = nullptr;
	blurConstantBuffer_->Map(0, nullptr, &mappedData);
	memcpy(mappedData, &blurParams_, sizeof(BlurParams));
	blurConstantBuffer_->Unmap(0, nullptr);

	// 定数バッファビューを設定
	cmdList->SetGraphicsRootConstantBufferView(2, blurConstantBuffer_->GetGPUVirtualAddress());

	// フルスクリーン三角形を描画
	cmdList->DrawInstanced(3, 1, 0, 0);

	// レンダーターゲットを終了
	outputRT->EndRender();
}

void PostProcessManager::RenderFinalComposite(RenderTexture* sceneTexture, RenderTexture* bloomTexture, RenderTexture* outputRT)
{
	auto cmdList = dxCommon_->GetCommandList();
	auto device = dxCommon_->GetDevice();

	if (outputRT) {
		outputRT->BeginRender();
	}
	else {
		// バックバッファをレンダーターゲットとして設定
		UINT backBufferIndex = dxCommon_->GetCurrentBackBufferIndex();
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dxCommon_->GetCPUDescriptorHandle(dxCommon_->GetRTVDescriptorHeap(), dxCommon_->GetDescriptorSizeRTV(), backBufferIndex);
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVDescriptorHeap()->GetCPUDescriptorHandleForHeapStart();
		cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
	}

	// ビューポートとシザー矩形を設定
	cmdList->RSSetViewports(1, &viewport_);
	cmdList->RSSetScissorRects(1, &scissorRect_);

	// パイプラインステートとルートシグネチャを設定
	cmdList->SetPipelineState(pipelineState_.Get());
	cmdList->SetGraphicsRootSignature(rootSignature_.Get());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ディスクリプタヒープを設定
	ID3D12DescriptorHeap* heaps[] = {
		srvManager_->GetSrvHeap(),
		dxCommon_->GetSamplerHeap()
	};
	cmdList->SetDescriptorHeaps(_countof(heaps), heaps);

	// シーンテクスチャとブルームテクスチャを設定
	cmdList->SetGraphicsRootDescriptorTable(0, sceneTexture->GetGPUHandle());  // t0
	cmdList->SetGraphicsRootDescriptorTable(1, bloomTexture->GetGPUHandle());  // t1
	cmdList->SetGraphicsRootDescriptorTable(2, dxCommon_->GetSamplerDescriptorHandle());

	// 各エフェクトを適用
	grayscaleEffect_->ApplyEffect(params_);
	vignetteEffect_->ApplyEffect(params_);
	noiseEffect_->ApplyEffect(params_);
	crtEffect_->ApplyEffect(params_);
	bloomEffect_->ApplyEffect(params_);
	tonemapEffect_->ApplyEffect(params_);

	// 定数バッファを更新
	UpdateConstantBuffer();
	cmdList->SetGraphicsRootConstantBufferView(3, constantBuffer_->GetGPUVirtualAddress());

	// フルスクリーン三角形を描画
	cmdList->DrawInstanced(3, 1, 0, 0);

	if (outputRT) {
		outputRT->EndRender();
	}
}

void PostProcessManager::SetBloomRenderTargets(RenderTexture* brightPassRT, RenderTexture* blurRT0, RenderTexture* blurRT1)
{
	// ブルーム用レンダーターゲットを設定
	brightPassRT_ = brightPassRT;
	blurRT_[0] = blurRT0;
	blurRT_[1] = blurRT1;
}

void PostProcessManager::Resize(uint32_t width, uint32_t height)
{
	// ビューポートとシザー矩形の設定を更新する
	viewport_.Width = static_cast<float>(width);
	viewport_.Height = static_cast<float>(height);
	scissorRect_.right = static_cast<LONG>(width);
	scissorRect_.bottom = static_cast<LONG>(height);

	// ブルームの逆テクセルサイズを更新する
	if (bloomEffect_)
	{
		bloomEffect_->SetInvScreenSize({ 1.0f / width, 1.0f / height });
	}
}


void PostProcessManager::Draw(RenderTexture* inputTexture, RenderTexture* outputRT)
{
	// ブルームが有効かつ、必要なレンダーターゲットが存在する場合
	if (bloomEffect_->IsEnabled() && HasBloomRenderTargets())
	{
		// マルチパス・ブルーム処理
		RenderWithBloom(inputTexture, outputRT);
	}
	else
	{
		// 従来のシングルパス処理
		RenderSinglePass(inputTexture, outputRT);
	}
}

void PostProcessManager::RenderWithBloom(RenderTexture* inputTexture, RenderTexture* outputRT)
{
	// ブルームのマルチパス処理

	// 1. ブライトパス（明るい部分の抽出）
	RenderBrightPass(inputTexture, brightPassRT_);

	// 2. 水平方向ブラー
	RenderBlurPass(brightPassRT_, blurRT_[0], true);

	// 3. 垂直方向ブラー
	RenderBlurPass(blurRT_[0], blurRT_[1], false);

	// 4. 最終合成（シーン + ブルーム）
	RenderFinalComposite(inputTexture, blurRT_[1], outputRT);
}

bool PostProcessManager::HasBloomRenderTargets() const
{
	// ブルーム用の全レンダーターゲットが設定されているかチェック
	return brightPassRT_ != nullptr && blurRT_[0] != nullptr && blurRT_[1] != nullptr;
}

void PostProcessManager::RenderSinglePass(RenderTexture* inputTexture, RenderTexture* outputRT)
{
	auto cmdList = dxCommon_->GetCommandList();

	if (outputRT) {
		outputRT->BeginRender();
	}
	else {
		// バックバッファをレンダーターゲットとして設定
		UINT backBufferIndex = dxCommon_->GetCurrentBackBufferIndex();
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dxCommon_->GetCPUDescriptorHandle(dxCommon_->GetRTVDescriptorHeap(), dxCommon_->GetDescriptorSizeRTV(), backBufferIndex);
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVDescriptorHeap()->GetCPUDescriptorHandleForHeapStart();
		cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
	}

	// ビューポートとシザー矩形を設定
	cmdList->RSSetViewports(1, &viewport_);
	cmdList->RSSetScissorRects(1, &scissorRect_);

	// パイプラインステートとルートシグネチャを設定
	cmdList->SetPipelineState(pipelineState_.Get());
	cmdList->SetGraphicsRootSignature(rootSignature_.Get());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ディスクリプタヒープを設定（SRVヒープとサンプラーヒープ）
	ID3D12DescriptorHeap* heaps[] = {
		srvManager_->GetSrvHeap(),
		dxCommon_->GetSamplerHeap()
	};
	cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
	cmdList->SetGraphicsRootDescriptorTable(0, inputTexture->GetGPUHandle());
	cmdList->SetGraphicsRootDescriptorTable(1, blurRT_[1]->GetGPUHandle()); // ダミーのブルームテクスチャ
	cmdList->SetGraphicsRootDescriptorTable(2, dxCommon_->GetSamplerDescriptorHandle());

	// ポストプロセスの各エフェクトを適用
	grayscaleEffect_->ApplyEffect(params_);
	vignetteEffect_->ApplyEffect(params_);
	noiseEffect_->ApplyEffect(params_);
	crtEffect_->ApplyEffect(params_);
	bloomEffect_->ApplyEffect(params_);
	tonemapEffect_->ApplyEffect(params_);

	// 定数バッファを更新
	UpdateConstantBuffer();
	// 定数バッファビューを設定
	cmdList->SetGraphicsRootConstantBufferView(3, constantBuffer_->GetGPUVirtualAddress());
	// フルスクリーン三角形を描画
	cmdList->DrawInstanced(3, 1, 0, 0);

	if (outputRT) {
		outputRT->EndRender();
	}
}

void PostProcessManager::CreateConstantBuffer()
{
	// バッファサイズを256バイトアラインメント
	size_t bufferSize = (sizeof(PostEffectParams) + 255) & ~255;

	// ヒーププロパティの設定
	D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

	// 定数バッファの作成
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
	// 前フレームと同じパラメータなら更新しない
	if (params_ == preParams_)
	{
		return;
	}

	// 定数バッファにパラメータをコピー
	void* mappedData = nullptr;
	constantBuffer_->Map(0, nullptr, &mappedData);
	memcpy(mappedData, &params_, sizeof(PostEffectParams));
	constantBuffer_->Unmap(0, nullptr);

	// 前フレームのパラメータを更新
	preParams_ = params_;
}

bool PostProcessManager::ReloadShaders(std::string& outError)
{
	// まず全てのシェーダーをコンパイルし、1つでも失敗したら何も差し替えない。
	// 途中まで差し替えると、パスの一部だけ古いままの中途半端な状態になる。
	auto finalVS = dxCommon_->TryCompileShader(vsPath_, L"vs_6_0", &outError);
	if (!finalVS) { return false; }

	auto finalPS = dxCommon_->TryCompileShader(psPath_, L"ps_6_0", &outError);
	if (!finalPS) { return false; }

	auto postVS = dxCommon_->TryCompileShader(L"Resources/shaders/PostEffect.VS.hlsl", L"vs_6_0", &outError);
	if (!postVS) { return false; }

	auto brightPS = dxCommon_->TryCompileShader(L"Resources/shaders/BrightPass.PS.hlsl", L"ps_6_0", &outError);
	if (!brightPS) { return false; }

	auto blurPS = dxCommon_->TryCompileShader(L"Resources/shaders/GaussianBlur.PS.hlsl", L"ps_6_0", &outError);
	if (!blurPS) { return false; }

	// PSOの共通設定。ルートシグネチャは変わらないため作り直さない。
	auto makePsoDesc = [](ID3D12RootSignature* rootSignature, IDxcBlob* vs, IDxcBlob* ps, DXGI_FORMAT rtvFormat)
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
		desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		desc.pRootSignature = rootSignature;
		desc.RTVFormats[0] = rtvFormat;
		desc.NumRenderTargets = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleMask = UINT_MAX;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.InputLayout = { nullptr, 0 };
		desc.DepthStencilState.DepthEnable = FALSE;
		desc.DepthStencilState.StencilEnable = FALSE;
		desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		return desc;
	};

	Microsoft::WRL::ComPtr<ID3D12PipelineState> newFinalPSO;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> newBrightPassPSO;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> newBlurPSO;

	auto finalDesc = makePsoDesc(rootSignature_.Get(), finalVS.Get(), finalPS.Get(), kDisplayColorFormat);
	if (FAILED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&finalDesc, IID_PPV_ARGS(&newFinalPSO))))
	{
		outError = "最終合成パイプラインの生成に失敗しました";
		return false;
	}

	auto brightDesc = makePsoDesc(bloomRootSignature_.Get(), postVS.Get(), brightPS.Get(), kBloomBufferFormat);
	if (FAILED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&brightDesc, IID_PPV_ARGS(&newBrightPassPSO))))
	{
		outError = "ブライトパスパイプラインの生成に失敗しました";
		return false;
	}

	auto blurDesc = makePsoDesc(bloomRootSignature_.Get(), postVS.Get(), blurPS.Get(), kBloomBufferFormat);
	if (FAILED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&blurDesc, IID_PPV_ARGS(&newBlurPSO))))
	{
		outError = "ブラーパスパイプラインの生成に失敗しました";
		return false;
	}

	// ここまで来たら全て成功しているので、まとめて差し替える
	pipelineState_ = newFinalPSO;
	brightPassPSO_ = newBrightPassPSO;
	blurPSO_ = newBlurPSO;
	return true;
}

#ifdef USE_IMGUI
void PostProcessManager::RegisterDebugUI()
{
	DebugUIManager::GetInstance()->RegisterDebugUI(
		this, "Post Process", [this]() { this->DrawImGui(); }, DebugUIArea::Inspector);
}

void PostProcessManager::DrawImGui()
{
	if (ImGui::CollapsingHeader("Tonemap", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool enabled = tonemapEffect_->IsEnabled();
		if (ImGui::Checkbox("Enabled##Tonemap", &enabled))
		{
			tonemapEffect_->SetEnabled(enabled);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("メインRTはHDRです。無効にすると 1.0 を超えた輝度が\nそのまま出力されるため、確認用途以外では有効のままにしてください");
		}

		int mode = static_cast<int>(tonemapEffect_->GetMode());
		if (ImGui::Combo("Mode##Tonemap", &mode, "ACES\0Reinhard\0"))
		{
			tonemapEffect_->SetMode(static_cast<TonemapMode>(mode));
		}

		float exposure = tonemapEffect_->GetExposure();
		if (ImGui::DragFloat("Exposure", &exposure, 0.01f, 0.01f, 16.0f, "%.2f"))
		{
			tonemapEffect_->SetExposure(exposure);
		}
	}

	if (ImGui::CollapsingHeader("Bloom"))
	{
		bool enabled = bloomEffect_->IsEnabled();
		if (ImGui::Checkbox("Enabled##Bloom", &enabled))
		{
			bloomEffect_->SetEnabled(enabled);
		}

		float threshold = bloomEffect_->GetThreshold();
		if (ImGui::DragFloat("Threshold", &threshold, 0.01f, 0.0f, 16.0f, "%.2f"))
		{
			bloomEffect_->SetThreshold(threshold);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("HDR化したので 1.0 を超える閾値にも意味があります");
		}

		float intensity = bloomEffect_->GetIntensity();
		if (ImGui::DragFloat("Intensity", &intensity, 0.01f, 0.0f, 8.0f, "%.2f"))
		{
			bloomEffect_->SetIntensity(intensity);
		}

		float knee = bloomEffect_->GetThresholdKnee();
		if (ImGui::DragFloat("Threshold Knee", &knee, 0.01f, 0.0f, 1.0f, "%.2f"))
		{
			bloomEffect_->SetThresholdKnee(knee);
		}

		float radius = bloomEffect_->GetRadius();
		if (ImGui::DragFloat("Radius", &radius, 0.1f, 0.0f, 64.0f, "%.1f"))
		{
			bloomEffect_->SetRadius(radius);
		}
	}
}
#endif
} // namespace KCE
