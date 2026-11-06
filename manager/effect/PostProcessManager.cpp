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
#include "manager/editor/SettingsJson.h"
#include "manager/editor/SettingsStore.h"
#endif

namespace KCE
{
namespace
{
constexpr float kPostFxDragSpeed = 0.01f;
constexpr float kBlurRadiusDragSpeed = 0.1f;
constexpr float kMinUnitValue = 0.0f;
constexpr float kMaxUnitValue = 1.0f;
constexpr float kMaxBlurRadius = 32.0f;
constexpr float kMaxRadialStrength = 0.5f;
constexpr int kMinRadialSamples = 2;
constexpr int kMaxRadialSamples = 32;
constexpr float kMinGamma = 0.01f;
constexpr float kMaxColorMultiplier = 4.0f;
constexpr float kMinLift = -1.0f;
constexpr float kMaxScanlineCount = 2048.0f;
constexpr float kScanlineCountDragSpeed = 1.0f;
constexpr float kMaxDistortionStrength = 1.0f;
constexpr float kMaxChromAberrationOffset = 0.05f;
constexpr float kChromAberrationDragSpeed = 0.001f;
constexpr float kMaxGrainSize = 8.0f;
}

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
	gaussianBlurEffect_ = std::make_unique<GaussianBlurEffect>();
	diffusionEffect_ = std::make_unique<DiffusionEffect>();
	radialBlurEffect_ = std::make_unique<RadialBlurEffect>();
	colorGradingEffect_ = std::make_unique<ColorGradingEffect>();

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
	gaussianBlurEffect_->ApplyEffect(params_);
	diffusionEffect_->ApplyEffect(params_);
	radialBlurEffect_->ApplyEffect(params_);
	tonemapEffect_->ApplyEffect(params_);
	colorGradingEffect_->ApplyEffect(params_);

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


void PostProcessManager::Draw(RenderTexture* inputTexture, RenderTexture* outputRT, RenderTexture* selectiveBloomSource)
{
	// ブルームが有効かつ、必要なレンダーターゲットが存在する場合
	if (bloomEffect_->IsEnabled() && HasBloomRenderTargets())
	{
		// マルチパス・ブルーム処理
		RenderWithBloom(inputTexture, outputRT, selectiveBloomSource);
	}
	else
	{
		// 従来のシングルパス処理
		RenderSinglePass(inputTexture, outputRT);
	}
}

void PostProcessManager::RenderWithBloom(RenderTexture* inputTexture, RenderTexture* outputRT, RenderTexture* selectiveBloomSource)
{
	// ブルームのマルチパス処理

	// ぼかす元を決める。選択的ブルームなら明るさで拾わず、
	// 発光用RTに書かれたものだけを使う
	RenderTexture* blurSource = selectiveBloomSource;
	if (!blurSource)
	{
		RenderBrightPass(inputTexture, brightPassRT_);
		blurSource = brightPassRT_;
	}

	// ぼかす(水平→垂直)
	RenderBlurPass(blurSource, blurRT_[0], true);

	RenderBlurPass(blurRT_[0], blurRT_[1], false);

	// シーンにブルームを合成する
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
	gaussianBlurEffect_->ApplyEffect(params_);
	diffusionEffect_->ApplyEffect(params_);
	radialBlurEffect_->ApplyEffect(params_);
	tonemapEffect_->ApplyEffect(params_);
	colorGradingEffect_->ApplyEffect(params_);

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
	DebugUIManager::GetInstance()->RegisterSettingsPage(this, "レンダリング", "ポストプロセス", [this]() { this->DrawImGui(); });
	// 全エフェクトの、有効と各項目を保存する。読み込みは、キーが無い項目を既定のままにする
	SettingsStore::GetInstance()->RegisterItem(this, "postProcess",
		[this]()
		{
			nlohmann::json json;
			json["tonemap"] = { { "enabled", tonemapEffect_->IsEnabled() }, { "mode", static_cast<int>(tonemapEffect_->GetMode()) },
				{ "exposure", tonemapEffect_->GetExposure() } };
			json["bloom"] = { { "enabled", bloomEffect_->IsEnabled() }, { "selective", selectiveBloomModeEnabled_ },
				{ "threshold", bloomEffect_->GetThreshold() }, { "intensity", bloomEffect_->GetIntensity() },
				{ "knee", bloomEffect_->GetThresholdKnee() }, { "radius", bloomEffect_->GetRadius() } };
			json["gaussianBlur"] = { { "enabled", gaussianBlurEffect_->IsEnabled() }, { "radius", gaussianBlurEffect_->GetRadius() },
				{ "strength", gaussianBlurEffect_->GetStrength() } };
			json["diffusion"] = { { "enabled", diffusionEffect_->IsEnabled() }, { "radius", diffusionEffect_->GetRadius() },
				{ "intensity", diffusionEffect_->GetIntensity() } };
			const Vector2 center = radialBlurEffect_->GetCenter();
			json["radialBlur"] = { { "enabled", radialBlurEffect_->IsEnabled() }, { "center", nlohmann::json::array({ center.x, center.y }) },
				{ "strength", radialBlurEffect_->GetStrength() }, { "samples", radialBlurEffect_->GetSampleCount() },
				{ "blend", radialBlurEffect_->GetBlend() } };
			json["colorGrading"] = { { "enabled", colorGradingEffect_->IsEnabled() },
				{ "lift", SettingsJson::ToJson(colorGradingEffect_->GetLift()) }, { "gamma", SettingsJson::ToJson(colorGradingEffect_->GetGamma()) },
				{ "gain", SettingsJson::ToJson(colorGradingEffect_->GetGain()) }, { "saturation", colorGradingEffect_->GetSaturation() },
				{ "contrast", colorGradingEffect_->GetContrast() } };
			json["grayscale"] = { { "enabled", grayscaleEffect_->IsEnabled() }, { "intensity", grayscaleEffect_->GetIntensity() } };
			json["vignette"] = { { "enabled", vignetteEffect_->IsEnabled() }, { "intensity", vignetteEffect_->GetIntensity() },
				{ "radius", vignetteEffect_->GetRadius() }, { "softness", vignetteEffect_->GetSoftness() },
				{ "color", SettingsJson::ToJson(vignetteEffect_->GetColor()) } };
			json["noise"] = { { "enabled", noiseEffect_->IsEnabled() }, { "intensity", noiseEffect_->GetIntensity() },
				{ "grainSize", noiseEffect_->GetGrainSize() }, { "luminanceAffect", noiseEffect_->GetLuminanceAffect() } };
			json["crt"] = { { "enabled", crtEffect_->IsEnabled() }, { "scanline", crtEffect_->IsScanlineEnabled() != 0 },
				{ "scanlineIntensity", crtEffect_->GetScanlineIntensity() }, { "scanlineCount", crtEffect_->GetScanlineCount() },
				{ "distortion", crtEffect_->IsDistortionEnabled() != 0 }, { "distortionStrength", crtEffect_->GetDistortionStrength() },
				{ "chromaticAberration", crtEffect_->IsChromaticAberrationEnabled() != 0 },
				{ "chromaticAberrationOffset", crtEffect_->GetChromaticAberrationOffset() } };
			return json;
		},
		[this](const nlohmann::json& json)
		{
			using SettingsJson::Read;
			// 項目の入った object を探す。無ければ nullptr
			const auto section = [&json](const char* key) -> const nlohmann::json*
			{
				const auto found = json.find(key);
				return (found != json.end() && found->is_object()) ? &*found : nullptr;
			};
			if (const auto* s = section("tonemap"))
			{
				bool enabled = tonemapEffect_->IsEnabled();
				int mode = static_cast<int>(tonemapEffect_->GetMode());
				float exposure = tonemapEffect_->GetExposure();
				Read(*s, "enabled", enabled); Read(*s, "mode", mode); Read(*s, "exposure", exposure);
				tonemapEffect_->SetEnabled(enabled);
				if (mode == static_cast<int>(TonemapMode::ACES) || mode == static_cast<int>(TonemapMode::Reinhard))
				{
					tonemapEffect_->SetMode(static_cast<TonemapMode>(mode));
				}
				tonemapEffect_->SetExposure(exposure);
			}
			if (const auto* s = section("bloom"))
			{
				bool enabled = bloomEffect_->IsEnabled();
				float threshold = bloomEffect_->GetThreshold(), intensity = bloomEffect_->GetIntensity();
				float knee = bloomEffect_->GetThresholdKnee(), radius = bloomEffect_->GetRadius();
				Read(*s, "enabled", enabled); Read(*s, "selective", selectiveBloomModeEnabled_);
				Read(*s, "threshold", threshold); Read(*s, "intensity", intensity); Read(*s, "knee", knee); Read(*s, "radius", radius);
				bloomEffect_->SetEnabled(enabled); bloomEffect_->SetThreshold(threshold); bloomEffect_->SetIntensity(intensity);
				bloomEffect_->SetThresholdKnee(knee); bloomEffect_->SetRadius(radius);
			}
			if (const auto* s = section("gaussianBlur"))
			{
				bool enabled = gaussianBlurEffect_->IsEnabled();
				float radius = gaussianBlurEffect_->GetRadius(), strength = gaussianBlurEffect_->GetStrength();
				Read(*s, "enabled", enabled); Read(*s, "radius", radius); Read(*s, "strength", strength);
				gaussianBlurEffect_->SetEnabled(enabled); gaussianBlurEffect_->SetRadius(radius); gaussianBlurEffect_->SetStrength(strength);
			}
			if (const auto* s = section("diffusion"))
			{
				bool enabled = diffusionEffect_->IsEnabled();
				float radius = diffusionEffect_->GetRadius(), intensity = diffusionEffect_->GetIntensity();
				Read(*s, "enabled", enabled); Read(*s, "radius", radius); Read(*s, "intensity", intensity);
				diffusionEffect_->SetEnabled(enabled); diffusionEffect_->SetRadius(radius); diffusionEffect_->SetIntensity(intensity);
			}
			if (const auto* s = section("radialBlur"))
			{
				bool enabled = radialBlurEffect_->IsEnabled();
				Vector2 center = radialBlurEffect_->GetCenter();
				float strength = radialBlurEffect_->GetStrength(), blend = radialBlurEffect_->GetBlend();
				int samples = radialBlurEffect_->GetSampleCount();
				Read(*s, "enabled", enabled); SettingsJson::ReadFloats(*s, "center", 2, &center.x);
				Read(*s, "strength", strength); Read(*s, "samples", samples); Read(*s, "blend", blend);
				radialBlurEffect_->SetEnabled(enabled); radialBlurEffect_->SetCenter(center); radialBlurEffect_->SetStrength(strength);
				radialBlurEffect_->SetSampleCount(samples); radialBlurEffect_->SetBlend(blend);
			}
			if (const auto* s = section("colorGrading"))
			{
				bool enabled = colorGradingEffect_->IsEnabled();
				Vector3 lift = colorGradingEffect_->GetLift(), gamma = colorGradingEffect_->GetGamma(), gain = colorGradingEffect_->GetGain();
				float saturation = colorGradingEffect_->GetSaturation(), contrast = colorGradingEffect_->GetContrast();
				Read(*s, "enabled", enabled); Read(*s, "lift", lift); Read(*s, "gamma", gamma); Read(*s, "gain", gain);
				Read(*s, "saturation", saturation); Read(*s, "contrast", contrast);
				colorGradingEffect_->SetEnabled(enabled); colorGradingEffect_->SetLift(lift); colorGradingEffect_->SetGamma(gamma);
				colorGradingEffect_->SetGain(gain); colorGradingEffect_->SetSaturation(saturation); colorGradingEffect_->SetContrast(contrast);
			}
			if (const auto* s = section("grayscale"))
			{
				bool enabled = grayscaleEffect_->IsEnabled();
				float intensity = grayscaleEffect_->GetIntensity();
				Read(*s, "enabled", enabled); Read(*s, "intensity", intensity);
				grayscaleEffect_->SetEnabled(enabled); grayscaleEffect_->SetIntensity(intensity);
			}
			if (const auto* s = section("vignette"))
			{
				bool enabled = vignetteEffect_->IsEnabled();
				float intensity = vignetteEffect_->GetIntensity(), radius = vignetteEffect_->GetRadius(), softness = vignetteEffect_->GetSoftness();
				Vector3 color = vignetteEffect_->GetColor();
				Read(*s, "enabled", enabled); Read(*s, "intensity", intensity); Read(*s, "radius", radius);
				Read(*s, "softness", softness); Read(*s, "color", color);
				vignetteEffect_->SetEnabled(enabled); vignetteEffect_->SetIntensity(intensity); vignetteEffect_->SetRadius(radius);
				vignetteEffect_->SetSoftness(softness); vignetteEffect_->SetColor(color);
			}
			if (const auto* s = section("noise"))
			{
				bool enabled = noiseEffect_->IsEnabled();
				float intensity = noiseEffect_->GetIntensity(), grainSize = noiseEffect_->GetGrainSize(), luminance = noiseEffect_->GetLuminanceAffect();
				Read(*s, "enabled", enabled); Read(*s, "intensity", intensity); Read(*s, "grainSize", grainSize); Read(*s, "luminanceAffect", luminance);
				noiseEffect_->SetEnabled(enabled); noiseEffect_->SetIntensity(intensity); noiseEffect_->SetGrainSize(grainSize);
				noiseEffect_->SetLuminanceAffect(luminance);
			}
			if (const auto* s = section("crt"))
			{
				bool enabled = crtEffect_->IsEnabled();
				bool scanline = crtEffect_->IsScanlineEnabled() != 0, distortion = crtEffect_->IsDistortionEnabled() != 0;
				bool chroma = crtEffect_->IsChromaticAberrationEnabled() != 0;
				float scanlineIntensity = crtEffect_->GetScanlineIntensity(), scanlineCount = crtEffect_->GetScanlineCount();
				float distortionStrength = crtEffect_->GetDistortionStrength(), chromaOffset = crtEffect_->GetChromaticAberrationOffset();
				Read(*s, "enabled", enabled); Read(*s, "scanline", scanline); Read(*s, "distortion", distortion); Read(*s, "chromaticAberration", chroma);
				Read(*s, "scanlineIntensity", scanlineIntensity); Read(*s, "scanlineCount", scanlineCount);
				Read(*s, "distortionStrength", distortionStrength); Read(*s, "chromaticAberrationOffset", chromaOffset);
				crtEffect_->SetEnabled(enabled); crtEffect_->SetScanlineEnabled(scanline ? 1 : 0); crtEffect_->SetDistortionEnabled(distortion ? 1 : 0);
				crtEffect_->SetChromaticAberrationEnabled(chroma ? 1 : 0); crtEffect_->SetScanlineIntensity(scanlineIntensity);
				crtEffect_->SetScanlineCount(scanlineCount); crtEffect_->SetDistortionStrength(distortionStrength);
				crtEffect_->SetChromaticAberrationOffset(chromaOffset);
			}
		});
}

void PostProcessManager::DrawImGui()
{
	if (ImGui::CollapsingHeader("トーンマップ", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool enabled = tonemapEffect_->IsEnabled();
		if (ImGui::Checkbox("有効##Tonemap", &enabled))
		{
			tonemapEffect_->SetEnabled(enabled);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("メインRTはHDRです。無効にすると 1.0 を超えた輝度が\nそのまま出力されるため、確認用途以外では有効のままにしてください");
		}

		int mode = static_cast<int>(tonemapEffect_->GetMode());
		if (ImGui::Combo("方式##Tonemap", &mode, "ACES\0Reinhard\0"))
		{
			tonemapEffect_->SetMode(static_cast<TonemapMode>(mode));
		}

		float exposure = tonemapEffect_->GetExposure();
		if (ImGui::DragFloat("露出", &exposure, 0.01f, 0.01f, 16.0f, "%.2f"))
		{
			tonemapEffect_->SetExposure(exposure);
		}
	}

	if (ImGui::CollapsingHeader("ブルーム"))
	{
		bool enabled = bloomEffect_->IsEnabled();
		if (ImGui::Checkbox("有効##Bloom", &enabled))
		{
			bloomEffect_->SetEnabled(enabled);
		}

		bool selective = selectiveBloomModeEnabled_;
		if (ImGui::Checkbox("発光したものだけ光らせる##SelectiveBloom", &selective))
		{
			selectiveBloomModeEnabled_ = selective;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("ONにすると、しきい値で拾うのをやめて発光を有効にしたものだけが光る");
		}

		float threshold = bloomEffect_->GetThreshold();
		if (ImGui::DragFloat("しきい値", &threshold, 0.01f, 0.0f, 16.0f, "%.2f"))
		{
			bloomEffect_->SetThreshold(threshold);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("HDR化したので 1.0 を超える閾値にも意味があります");
		}

		float intensity = bloomEffect_->GetIntensity();
		if (ImGui::DragFloat("強さ", &intensity, 0.01f, 0.0f, 8.0f, "%.2f"))
		{
			bloomEffect_->SetIntensity(intensity);
		}

		float knee = bloomEffect_->GetThresholdKnee();
		if (ImGui::DragFloat("しきい値のなめらかさ", &knee, 0.01f, 0.0f, 1.0f, "%.2f"))
		{
			bloomEffect_->SetThresholdKnee(knee);
		}

		float radius = bloomEffect_->GetRadius();
		if (ImGui::DragFloat("半径", &radius, 0.1f, 0.0f, 64.0f, "%.1f"))
		{
			bloomEffect_->SetRadius(radius);
		}
	}

	if (ImGui::CollapsingHeader("ガウシアンブラー"))
	{
		bool enabled = gaussianBlurEffect_->IsEnabled();
		if (ImGui::Checkbox("有効##GaussianBlur", &enabled))
		{
			gaussianBlurEffect_->SetEnabled(enabled);
		}
		float radius = gaussianBlurEffect_->GetRadius();
		if (ImGui::DragFloat("半径##GaussianBlur", &radius, kBlurRadiusDragSpeed, kMinUnitValue, kMaxBlurRadius, "%.1f"))
		{
			gaussianBlurEffect_->SetRadius(radius);
		}
		float strength = gaussianBlurEffect_->GetStrength();
		if (ImGui::DragFloat("強さ##GaussianBlur", &strength, kPostFxDragSpeed, kMinUnitValue, kMaxUnitValue, "%.2f"))
		{
			gaussianBlurEffect_->SetStrength(strength);
		}
	}

	if (ImGui::CollapsingHeader("ディフュージョン"))
	{
		bool enabled = diffusionEffect_->IsEnabled();
		if (ImGui::Checkbox("有効##Diffusion", &enabled))
		{
			diffusionEffect_->SetEnabled(enabled);
		}
		float radius = diffusionEffect_->GetRadius();
		if (ImGui::DragFloat("半径##Diffusion", &radius, kBlurRadiusDragSpeed, kMinUnitValue, kMaxBlurRadius, "%.1f"))
		{
			diffusionEffect_->SetRadius(radius);
		}
		float intensity = diffusionEffect_->GetIntensity();
		if (ImGui::DragFloat("強さ##Diffusion", &intensity, kPostFxDragSpeed, kMinUnitValue, kMaxUnitValue, "%.2f"))
		{
			diffusionEffect_->SetIntensity(intensity);
		}
	}

	if (ImGui::CollapsingHeader("ラジアルブラー"))
	{
		bool enabled = radialBlurEffect_->IsEnabled();
		if (ImGui::Checkbox("有効##RadialBlur", &enabled))
		{
			radialBlurEffect_->SetEnabled(enabled);
		}
		Vector2 center = radialBlurEffect_->GetCenter();
		float centerValues[] = { center.x, center.y };
		if (ImGui::DragFloat2("中心##RadialBlur", centerValues, kPostFxDragSpeed, kMinUnitValue, kMaxUnitValue, "%.2f"))
		{
			radialBlurEffect_->SetCenter({ centerValues[0], centerValues[1] });
		}
		float strength = radialBlurEffect_->GetStrength();
		if (ImGui::DragFloat("強さ##RadialBlur", &strength, kPostFxDragSpeed, kMinUnitValue, kMaxRadialStrength, "%.3f"))
		{
			radialBlurEffect_->SetStrength(strength);
		}
		int samples = radialBlurEffect_->GetSampleCount();
		if (ImGui::SliderInt("サンプル数##RadialBlur", &samples, kMinRadialSamples, kMaxRadialSamples))
		{
			radialBlurEffect_->SetSampleCount(samples);
		}
		float blend = radialBlurEffect_->GetBlend();
		if (ImGui::DragFloat("混ぜ具合##RadialBlur", &blend, kPostFxDragSpeed, kMinUnitValue, kMaxUnitValue, "%.2f"))
		{
			radialBlurEffect_->SetBlend(blend);
		}
	}

	if (ImGui::CollapsingHeader("カラーグレーディング"))
	{
		bool enabled = colorGradingEffect_->IsEnabled();
		if (ImGui::Checkbox("有効##ColorGrading", &enabled))
		{
			colorGradingEffect_->SetEnabled(enabled);
		}
		Vector3 lift = colorGradingEffect_->GetLift();
		float liftValues[] = { lift.x, lift.y, lift.z };
		if (ImGui::DragFloat3("リフト", liftValues, kPostFxDragSpeed, kMinLift, kMaxUnitValue, "%.2f"))
		{
			colorGradingEffect_->SetLift({ liftValues[0], liftValues[1], liftValues[2] });
		}
		Vector3 gamma = colorGradingEffect_->GetGamma();
		float gammaValues[] = { gamma.x, gamma.y, gamma.z };
		if (ImGui::DragFloat3("ガンマ", gammaValues, kPostFxDragSpeed, kMinGamma, kMaxColorMultiplier, "%.2f"))
		{
			colorGradingEffect_->SetGamma({ gammaValues[0], gammaValues[1], gammaValues[2] });
		}
		Vector3 gain = colorGradingEffect_->GetGain();
		float gainValues[] = { gain.x, gain.y, gain.z };
		if (ImGui::DragFloat3("ゲイン", gainValues, kPostFxDragSpeed, kMinUnitValue, kMaxColorMultiplier, "%.2f"))
		{
			colorGradingEffect_->SetGain({ gainValues[0], gainValues[1], gainValues[2] });
		}
		float saturation = colorGradingEffect_->GetSaturation();
		if (ImGui::DragFloat("彩度", &saturation, kPostFxDragSpeed, kMinUnitValue, kMaxColorMultiplier, "%.2f"))
		{
			colorGradingEffect_->SetSaturation(saturation);
		}
		float contrast = colorGradingEffect_->GetContrast();
		if (ImGui::DragFloat("コントラスト", &contrast, kPostFxDragSpeed, kMinUnitValue, kMaxColorMultiplier, "%.2f"))
		{
			colorGradingEffect_->SetContrast(contrast);
		}
	}

	if (ImGui::CollapsingHeader("グレースケール"))
	{
		bool enabled = grayscaleEffect_->IsEnabled();
		if (ImGui::Checkbox("有効##Grayscale", &enabled))
		{
			grayscaleEffect_->SetEnabled(enabled);
		}
		float intensity = grayscaleEffect_->GetIntensity();
		if (ImGui::DragFloat("強さ##Grayscale", &intensity, kPostFxDragSpeed, kMinUnitValue, kMaxUnitValue, "%.2f"))
		{
			grayscaleEffect_->SetIntensity(intensity);
		}
	}

	if (ImGui::CollapsingHeader("ビネット"))
	{
		bool enabled = vignetteEffect_->IsEnabled();
		if (ImGui::Checkbox("有効##Vignette", &enabled))
		{
			vignetteEffect_->SetEnabled(enabled);
		}
		float intensity = vignetteEffect_->GetIntensity();
		if (ImGui::DragFloat("強さ##Vignette", &intensity, kPostFxDragSpeed, kMinUnitValue, kMaxUnitValue, "%.2f"))
		{
			vignetteEffect_->SetIntensity(intensity);
		}
		float radius = vignetteEffect_->GetRadius();
		if (ImGui::DragFloat("半径##Vignette", &radius, kPostFxDragSpeed, kMinUnitValue, kMaxUnitValue, "%.2f"))
		{
			vignetteEffect_->SetRadius(radius);
		}
		float softness = vignetteEffect_->GetSoftness();
		if (ImGui::DragFloat("柔らかさ##Vignette", &softness, kPostFxDragSpeed, kMinUnitValue, kMaxUnitValue, "%.2f"))
		{
			vignetteEffect_->SetSoftness(softness);
		}
		Vector3 color = vignetteEffect_->GetColor();
		float colorValues[] = { color.x, color.y, color.z };
		if (ImGui::ColorEdit3("色##Vignette", colorValues))
		{
			vignetteEffect_->SetColor({ colorValues[0], colorValues[1], colorValues[2] });
		}
	}

	if (ImGui::CollapsingHeader("ノイズ"))
	{
		bool enabled = noiseEffect_->IsEnabled();
		if (ImGui::Checkbox("有効##Noise", &enabled))
		{
			noiseEffect_->SetEnabled(enabled);
		}
		float intensity = noiseEffect_->GetIntensity();
		if (ImGui::DragFloat("強さ##Noise", &intensity, kPostFxDragSpeed, kMinUnitValue, kMaxUnitValue, "%.2f"))
		{
			noiseEffect_->SetIntensity(intensity);
		}
		float grainSize = noiseEffect_->GetGrainSize();
		if (ImGui::DragFloat("粒の大きさ##Noise", &grainSize, kPostFxDragSpeed, kMinUnitValue, kMaxGrainSize, "%.2f"))
		{
			noiseEffect_->SetGrainSize(grainSize);
		}
		float luminanceAffect = noiseEffect_->GetLuminanceAffect();
		if (ImGui::DragFloat("暗部への効き##Noise", &luminanceAffect, kPostFxDragSpeed, kMinUnitValue, kMaxUnitValue, "%.2f"))
		{
			noiseEffect_->SetLuminanceAffect(luminanceAffect);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("時間を送っている箇所が無いので、今はノイズが動きません");
		}
	}

	if (ImGui::CollapsingHeader("CRT"))
	{
		bool enabled = crtEffect_->IsEnabled();
		if (ImGui::Checkbox("有効##Crt", &enabled))
		{
			crtEffect_->SetEnabled(enabled);
		}

		// 以下の3つは CRT 全体が有効なときだけ効く
		bool scanline = crtEffect_->IsScanlineEnabled();
		if (ImGui::Checkbox("走査線##Crt", &scanline))
		{
			crtEffect_->SetScanlineEnabled(scanline ? 1 : 0);
		}
		float scanlineIntensity = crtEffect_->GetScanlineIntensity();
		if (ImGui::DragFloat("走査線の強さ##Crt", &scanlineIntensity, kPostFxDragSpeed, kMinUnitValue, kMaxUnitValue, "%.2f"))
		{
			crtEffect_->SetScanlineIntensity(scanlineIntensity);
		}
		float scanlineCount = crtEffect_->GetScanlineCount();
		if (ImGui::DragFloat("走査線の本数##Crt", &scanlineCount, kScanlineCountDragSpeed, kMinUnitValue, kMaxScanlineCount, "%.0f"))
		{
			crtEffect_->SetScanlineCount(scanlineCount);
		}

		bool distortion = crtEffect_->IsDistortionEnabled();
		if (ImGui::Checkbox("樽型の歪み##Crt", &distortion))
		{
			crtEffect_->SetDistortionEnabled(distortion ? 1 : 0);
		}
		float distortionStrength = crtEffect_->GetDistortionStrength();
		if (ImGui::DragFloat("歪みの強さ##Crt", &distortionStrength, kPostFxDragSpeed, kMinUnitValue, kMaxDistortionStrength, "%.2f"))
		{
			crtEffect_->SetDistortionStrength(distortionStrength);
		}

		bool chrom = crtEffect_->IsChromaticAberrationEnabled();
		if (ImGui::Checkbox("色収差##Crt", &chrom))
		{
			crtEffect_->SetChromaticAberrationEnabled(chrom ? 1 : 0);
		}
		float chromOffset = crtEffect_->GetChromaticAberrationOffset();
		if (ImGui::DragFloat("色収差のずれ量##Crt", &chromOffset, kChromAberrationDragSpeed, kMinUnitValue, kMaxChromAberrationOffset, "%.3f"))
		{
			crtEffect_->SetChromaticAberrationOffset(chromOffset);
		}
	}
}
#endif
} // namespace KCE
