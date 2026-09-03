#include "LightPassPipeline.h"
#include "base/DirectXCommon.h"
#include "base/Logger.h"
#include "DirectXTex/d3dx12.h"
#include <cassert>
#include <d3dcompiler.h>
#include <filesystem>
#include <vector>

namespace KCE
{
#pragma comment(lib, "d3dcompiler.lib")

void LightPassPipeline::Initialize(DirectXCommon* dxCommon)
{
	assert(dxCommon);
	dxCommon_ = dxCommon;

	CreateRootSignature();
	CreatePipelineState(true);
	CreatePipelineState(false);

	KCE::Logger::Log("LightPassPipeline initialized\n");
}

void LightPassPipeline::CreateRootSignature()
{
	auto* device = dxCommon_->GetDevice();

	// ルートパラメータ（フル機能版）
	// 0: CameraData (CBV, b0)
	// 1: DirectionalLightData (CBV, b1)
	// 2: CascadeShadowData (CBV, b2)
	// 3: LightBuffer (CBV, b3) - SpotLights + PointLights
	// 4: G-Buffer (SRV Table, t0-t4)
	// 5-8: CascadeShadowMaps (SRV, t5-t8)
	// 9-16: SpotLightShadowMaps (SRV, t9-t16) - 8個
	// 17-18: PointLightShadowMaps Cubemaps (SRV, t17-t18)

	// G-Buffer用SRVレンジ
	D3D12_DESCRIPTOR_RANGE gBufferRange = {};
	gBufferRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	gBufferRange.NumDescriptors = 5;
	gBufferRange.BaseShaderRegister = 0;
	gBufferRange.RegisterSpace = 0;
	gBufferRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// 各シャドウマップ用レンジ
	D3D12_DESCRIPTOR_RANGE cascadeRanges[4] = {};
	for (int i = 0; i < 4; ++i)
	{
		cascadeRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		cascadeRanges[i].NumDescriptors = 1;
		cascadeRanges[i].BaseShaderRegister = 5 + i; // t5-t8
		cascadeRanges[i].RegisterSpace = 0;
		cascadeRanges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	}

	D3D12_DESCRIPTOR_RANGE spotRanges[8] = {};
	for (int i = 0; i < 8; ++i)
	{
		spotRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		spotRanges[i].NumDescriptors = 1;
		spotRanges[i].BaseShaderRegister = 9 + i; // t9-t16
		spotRanges[i].RegisterSpace = 0;
		spotRanges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	}

	D3D12_DESCRIPTOR_RANGE pointRanges[2] = {};
	for (int i = 0; i < 2; ++i)
	{
		pointRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		pointRanges[i].NumDescriptors = 1;
		pointRanges[i].BaseShaderRegister = 17 + i; // t17-t18
		pointRanges[i].RegisterSpace = 0;
		pointRanges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	}

	D3D12_ROOT_PARAMETER rootParams[19] = {};  // 4 CBVs + 1 GBuffer + 4 Cascade + 8 Spot + 2 Point = 19

	// CBVs (0-3)
	for (int i = 0; i < 4; ++i)
	{
		rootParams[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[i].Descriptor.ShaderRegister = i;
		rootParams[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	}

	// G-Buffer SRV Table (4)
	rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[4].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[4].DescriptorTable.pDescriptorRanges = &gBufferRange;
	rootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// Cascade Shadow Maps (5-8)
	for (int i = 0; i < 4; ++i)
	{
		rootParams[5 + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[5 + i].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[5 + i].DescriptorTable.pDescriptorRanges = &cascadeRanges[i];
		rootParams[5 + i].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	}

	// SpotLight Shadow Maps (9-16) - 8個
	for (int i = 0; i < 8; ++i)
	{
		rootParams[9 + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[9 + i].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[9 + i].DescriptorTable.pDescriptorRanges = &spotRanges[i];
		rootParams[9 + i].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	}

	// PointLight Shadow Maps Cubemaps (17-18)
	for (int i = 0; i < 2; ++i)
	{
		rootParams[17 + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[17 + i].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[17 + i].DescriptorTable.pDescriptorRanges = &pointRanges[i];
		rootParams[17 + i].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	}

	// サンプラー
	D3D12_STATIC_SAMPLER_DESC samplers[2] = {};

	// 通常サンプラー (s0)
	samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[0].MipLODBias = 0.0f;
	samplers[0].MaxAnisotropy = 1;
	samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplers[0].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	samplers[0].MinLOD = 0.0f;
	samplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	samplers[0].ShaderRegister = 0;
	samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// シャドウ比較サンプラー (s1)
	samplers[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	samplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samplers[1].MipLODBias = 0.0f;
	samplers[1].MaxAnisotropy = 1;
	samplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	samplers[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	samplers[1].MinLOD = 0.0f;
	samplers[1].MaxLOD = D3D12_FLOAT32_MAX;
	samplers[1].ShaderRegister = 1;
	samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC desc = {};
	desc.NumParameters = _countof(rootParams);
	desc.pParameters = rootParams;
	desc.NumStaticSamplers = _countof(samplers);
	desc.pStaticSamplers = samplers;
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			KCE::Logger::Log(static_cast<char*>(errorBlob->GetBufferPointer()));
		}
		assert(false);
	}

	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
}

void LightPassPipeline::CreatePipelineState(bool bloomTargetEnabled)
{
	auto* device = dxCommon_->GetDevice();

	// 頂点シェーダーのコンパイル
	Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

	// ファイルが存在しない場合の自動検索
	std::wstring vsPath = L"Resources/shaders/LightPass.VS.hlsl";
	if (!std::filesystem::exists(vsPath))
	{
		std::vector<std::wstring> searchPaths = {
			L"engine/Resources/shaders/LightPass.VS.hlsl",
			L"Resources/shaders/LightPass.VS.hlsl",
			L"../Resources/shaders/LightPass.VS.hlsl"
		};
		for (const auto& path : searchPaths)
		{
			if (std::filesystem::exists(path))
			{
				vsPath = path;
				break;
			}
		}
	}

	HRESULT hr = D3DCompileFromFile(
		vsPath.c_str(),
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main", "vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, &vsBlob, &errorBlob
	);
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			KCE::Logger::Log(static_cast<char*>(errorBlob->GetBufferPointer()));
		}
		assert(false);
	}

	// ピクセルシェーダーのコンパイル
	Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
	// ファイルが存在しない場合の自動検索
	std::wstring psPath = L"Resources/shaders/LightPass.PS.hlsl";
	if (!std::filesystem::exists(psPath))
	{
		std::vector<std::wstring> searchPaths = {
			L"engine/Resources/shaders/LightPass.PS.hlsl",
			L"Resources/shaders/LightPass.PS.hlsl",
			L"../Resources/shaders/LightPass.PS.hlsl"
		};
		for (const auto& path : searchPaths)
		{
			if (std::filesystem::exists(path))
			{
				psPath = path;
				break;
			}
		}
	}

	const D3D_SHADER_MACRO singleTargetDefines[] = {
		{ "KCE_BLOOM_TARGET_DISABLED", "1" },
		{ nullptr, nullptr }
	};
	hr = D3DCompileFromFile(
		psPath.c_str(),
		bloomTargetEnabled ? nullptr : singleTargetDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, &psBlob, &errorBlob
	);
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			KCE::Logger::Log(static_cast<char*>(errorBlob->GetBufferPointer()));
		}
		assert(false);
	}

	// PSO作成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = rootSignature_.Get();
	psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
	psoDesc.InputLayout = { nullptr, 0 };
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	psoDesc.NumRenderTargets = bloomTargetEnabled ? 2 : 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	if (bloomTargetEnabled)
	{
		psoDesc.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	}
	psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = UINT_MAX;

	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
	psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	psoDesc.RasterizerState.DepthClipEnable = FALSE;

	psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	if (bloomTargetEnabled)
	{
		psoDesc.BlendState.RenderTarget[1].BlendEnable = FALSE;
		psoDesc.BlendState.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}

	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.StencilEnable = FALSE;

	auto& target = bloomTargetEnabled ? pipelineState_ : singleTargetPipelineState_;
	hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&target));
	assert(SUCCEEDED(hr));
}

void LightPassPipeline::SetPipeline(bool bloomTargetEnabled)
{
	auto* commandList = dxCommon_->GetCommandList();
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(
		bloomTargetEnabled ? pipelineState_.Get() : singleTargetPipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
}
} // namespace KCE
