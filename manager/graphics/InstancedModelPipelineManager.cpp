#include "InstancedModelPipelineManager.h"
#include "base/Logger.h"
#include "externals/DirectXTex/d3dx12.h"
#include <d3dcompiler.h>
#include <cassert>

InstancedModelPipelineManager* InstancedModelPipelineManager::GetInstance()
{
	static InstancedModelPipelineManager instance;
	return &instance;
}

void InstancedModelPipelineManager::Initialize(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;
	CreatePipeline();
}

void InstancedModelPipelineManager::Finalize()
{
	rootSignature_.Reset();
	pipelineState_.Reset();
	pipelineStateGBuffer_.Reset();
	pipelineStateShadow_.Reset();
}

void InstancedModelPipelineManager::CreatePipeline()
{
	HRESULT hr;
	ID3D12Device* device = dxCommon_->GetDevice();

	//----- ルートシグネチャの生成 -----//

	// 通常サンプラー + シャドウ比較サンプラー
	D3D12_STATIC_SAMPLER_DESC staticSamplers[2] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	staticSamplers[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	staticSamplers[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	staticSamplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	staticSamplers[1].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[1].ShaderRegister = 1;
	staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// ディスクリプタレンジ
	CD3DX12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE descriptorRangeEnvMap[1] = {};
	descriptorRangeEnvMap[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

	CD3DX12_DESCRIPTOR_RANGE descriptorRangeShadowMap[1] = {};
	descriptorRangeShadowMap[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);

	CD3DX12_DESCRIPTOR_RANGE descriptorRangeCascade[4] = {};
	for (int i = 0; i < 4; ++i)
	{
		descriptorRangeCascade[i].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6 + i);
	}

	CD3DX12_ROOT_PARAMETER rootParameters[16] = {};

	rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);
	rootParameters[1].InitAsShaderResourceView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParameters[2].InitAsDescriptorTable(1, &descriptorRange[0], D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[3].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[4].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[5].InitAsShaderResourceView(3, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[6].InitAsShaderResourceView(4, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[7].InitAsConstantBufferView(5, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[8].InitAsDescriptorTable(1, &descriptorRangeEnvMap[0], D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[9].InitAsDescriptorTable(1, &descriptorRangeShadowMap[0], D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[10].InitAsConstantBufferView(6, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[11].InitAsConstantBufferView(7, 0, D3D12_SHADER_VISIBILITY_PIXEL);

	for (int i = 0; i < 4; ++i)
	{
		rootParameters[12 + i].InitAsDescriptorTable(1, &descriptorRangeCascade[i], D3D12_SHADER_VISIBILITY_PIXEL);
	}

	CD3DX12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.Init(_countof(rootParameters), rootParameters, _countof(staticSamplers), staticSamplers, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			KCE::Logger::Log(reinterpret_cast<const char*>(errorBlob->GetBufferPointer()));
		}
		assert(false);
	}
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));

	//----- パイプラインステートの生成 -----//

	auto vsBlob = dxCommon_->CompileSharder(L"Resources/shaders/InstancedObject3d.VS.hlsl", L"vs_6_0");
	auto psBlob = dxCommon_->CompileSharder(L"Resources/shaders/InstancedObject3d.PS.hlsl", L"ps_6_0");
	assert(vsBlob && "Failed to compile VS");
	assert(psBlob && "Failed to compile PS");

	D3D12_INPUT_ELEMENT_DESC inputElements[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = rootSignature_.Get();
	psoDesc.InputLayout = { inputElements, _countof(inputElements) };
	psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

	/** アルファブレンド設定 */
	D3D12_RENDER_TARGET_BLEND_DESC& blend = psoDesc.BlendState.RenderTarget[0];
	blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blend.BlendEnable = TRUE;
	blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blend.BlendOp = D3D12_BLEND_OP_ADD;
	blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blend.SrcBlendAlpha = D3D12_BLEND_ONE;
	blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blend.DestBlendAlpha = D3D12_BLEND_ZERO;

	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));

	//----- GBuffer用パイプラインステートの生成 -----//
	auto vsGBufferBlob = dxCommon_->CompileSharder(L"Resources/shaders/InstancedGBufferPass.VS.hlsl", L"vs_6_0");
	auto psGBufferBlob = dxCommon_->CompileSharder(L"Resources/shaders/GBufferPass.PS.hlsl", L"ps_6_0");
	assert(vsGBufferBlob && "Failed to compile VS for Instanced GBuffer");
	assert(psGBufferBlob && "Failed to compile PS for GBuffer");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gbufferPsoDesc = psoDesc;
	gbufferPsoDesc.VS = { vsGBufferBlob->GetBufferPointer(), vsGBufferBlob->GetBufferSize() };
	gbufferPsoDesc.PS = { psGBufferBlob->GetBufferPointer(), psGBufferBlob->GetBufferSize() };

	gbufferPsoDesc.NumRenderTargets = 4;
	gbufferPsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;   // Albedo
	gbufferPsoDesc.RTVFormats[1] = DXGI_FORMAT_R10G10B10A2_UNORM; // Normal
	gbufferPsoDesc.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;   // Material
	gbufferPsoDesc.RTVFormats[3] = DXGI_FORMAT_R8G8B8A8_UNORM;   // Emissive
	gbufferPsoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	for (int i = 0; i < 4; ++i)
	{
		gbufferPsoDesc.BlendState.RenderTarget[i].BlendEnable = FALSE;
		gbufferPsoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}

	hr = device->CreateGraphicsPipelineState(&gbufferPsoDesc, IID_PPV_ARGS(&pipelineStateGBuffer_));
	assert(SUCCEEDED(hr));

	//----- Shadow用パイプラインステートの生成 -----//
	auto vsShadowBlob = dxCommon_->CompileSharder(L"Resources/shaders/InstancedShadowMap.VS.hlsl", L"vs_6_0");
	auto psShadowBlob = dxCommon_->CompileSharder(L"Resources/shaders/ShadowMap.PS.hlsl", L"ps_6_0");
	assert(vsShadowBlob && "Failed to compile VS for Instanced Shadow Map");
	assert(psShadowBlob && "Failed to compile PS for Shadow Map");

	D3D12_INPUT_ELEMENT_DESC shadowInputElementDescs[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc = {};
	shadowPsoDesc.pRootSignature = rootSignature_.Get();
	shadowPsoDesc.InputLayout = { shadowInputElementDescs, _countof(shadowInputElementDescs) };
	shadowPsoDesc.VS = { vsShadowBlob->GetBufferPointer(), vsShadowBlob->GetBufferSize() };
	shadowPsoDesc.PS = { psShadowBlob->GetBufferPointer(), psShadowBlob->GetBufferSize() };

	shadowPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	shadowPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

	shadowPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	shadowPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	shadowPsoDesc.BlendState.AlphaToCoverageEnable = FALSE;
	shadowPsoDesc.BlendState.IndependentBlendEnable = FALSE;
	shadowPsoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	shadowPsoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0;

	shadowPsoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	shadowPsoDesc.NumRenderTargets = 0;
	shadowPsoDesc.SampleMask = UINT_MAX;
	shadowPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	shadowPsoDesc.SampleDesc.Count = 1;

	hr = device->CreateGraphicsPipelineState(&shadowPsoDesc, IID_PPV_ARGS(&pipelineStateShadow_));
	assert(SUCCEEDED(hr));
}
