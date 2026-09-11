#include "graphics/postfx/FullscreenPass.h"

#include <algorithm>

#include "DirectXTex/d3dx12.h"
#include "base/DirectXCommon.h"

namespace KCE
{
namespace
{
constexpr UINT kRootParamConstants = 0;
constexpr UINT kRootParamFirstSrv = 1;
constexpr UINT kSamplerLinear = 0;
constexpr UINT kSamplerPoint = 1;
constexpr UINT kSamplerShadow = 2;
constexpr UINT kShadowMaxAnisotropy = 16;
constexpr UINT kFullscreenVertexCount = 3;
} // namespace

bool FullscreenPass::Create(DirectXCommon* dxCommon, const FullscreenPassDesc& desc, std::string& outError)
{
	dxCommon_ = dxCommon;
	desc_ = desc;
	if (desc_.srvCount > kMaxSrvCount)
	{
		outError = "SRV が多すぎます";
		return false;
	}
	return CreateRootSignature(outError) && CreatePipeline(outError);
}

bool FullscreenPass::ReloadShaders(std::string& outError)
{
	return CreatePipeline(outError);
}

bool FullscreenPass::CreateRootSignature(std::string& outError)
{
	CD3DX12_DESCRIPTOR_RANGE ranges[kMaxSrvCount]{};
	CD3DX12_ROOT_PARAMETER params[kRootParamFirstSrv + kMaxSrvCount]{};
	params[kRootParamConstants].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	for (uint32_t i = 0; i < desc_.srvCount; ++i)
	{
		ranges[i].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, i);
		params[kRootParamFirstSrv + i].InitAsDescriptorTable(1, &ranges[i], D3D12_SHADER_VISIBILITY_PIXEL);
	}

	const CD3DX12_STATIC_SAMPLER_DESC samplers[] = {
		CD3DX12_STATIC_SAMPLER_DESC(kSamplerLinear, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
		CD3DX12_STATIC_SAMPLER_DESC(kSamplerPoint, D3D12_FILTER_MIN_MAG_MIP_POINT,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
		// シャドウマップの外は「影なし」にしたいので、境界色を白（最遠）にする
		CD3DX12_STATIC_SAMPLER_DESC(kSamplerShadow, D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
			D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER,
			0.0f, kShadowMaxAnisotropy, D3D12_COMPARISON_FUNC_LESS_EQUAL, D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE),
	};

	CD3DX12_ROOT_SIGNATURE_DESC desc{};
	desc.Init(kRootParamFirstSrv + desc_.srvCount, params, _countof(samplers), samplers, D3D12_ROOT_SIGNATURE_FLAG_NONE);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob)))
	{
		outError = errorBlob ? static_cast<const char*>(errorBlob->GetBufferPointer()) : "ルートシグネチャのシリアライズに失敗";
		return false;
	}
	if (FAILED(dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_))))
	{
		outError = "ルートシグネチャの生成に失敗";
		return false;
	}
	return true;
}

bool FullscreenPass::CreatePipeline(std::string& outError)
{
	if (!rootSignature_)
	{
		outError = "ルートシグネチャがありません";
		return false;
	}

	auto vs = dxCommon_->TryCompileShader(L"Resources/shaders/PostEffect.VS.hlsl", L"vs_6_0", &outError);
	if (!vs) { return false; }
	auto ps = dxCommon_->TryCompileShader(desc_.pixelShaderPath, L"ps_6_0", &outError);
	if (!ps) { return false; }

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature_.Get();
	desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	desc.InputLayout = { nullptr, 0 };
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = desc_.rtvFormat;
	desc.SampleDesc.Count = 1;
	desc.SampleMask = UINT_MAX;
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	// 深度はシェーダーで読むので、深度テスト・書き込みは行わない
	desc.DepthStencilState.DepthEnable = FALSE;
	desc.DepthStencilState.StencilEnable = FALSE;
	desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	auto& blend = desc.BlendState.RenderTarget[0];
	if (desc_.blend != FullscreenBlend::Opaque)
	{
		blend.BlendEnable = TRUE;
		blend.SrcBlend = desc_.blend == FullscreenBlend::Additive ? D3D12_BLEND_ONE : D3D12_BLEND_SRC_ALPHA;
		blend.DestBlend = desc_.blend == FullscreenBlend::Additive ? D3D12_BLEND_ONE : D3D12_BLEND_INV_SRC_ALPHA;
		blend.BlendOp = D3D12_BLEND_OP_ADD;
		// 出力先のアルファは触らない
		blend.SrcBlendAlpha = D3D12_BLEND_ZERO;
		blend.DestBlendAlpha = D3D12_BLEND_ONE;
		blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	}

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline;
	if (FAILED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline))))
	{
		outError = "全画面パスのパイプラインの生成に失敗";
		return false;
	}
	pipelineState_ = pipeline;
	return true;
}

void FullscreenPass::Draw(D3D12_GPU_VIRTUAL_ADDRESS constants, const D3D12_GPU_DESCRIPTOR_HANDLE* srvs, uint32_t srvCount)
{
	if (!pipelineState_)
	{
		return;
	}
	auto* commandList = dxCommon_->GetCommandList();
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	if (constants != 0)
	{
		commandList->SetGraphicsRootConstantBufferView(kRootParamConstants, constants);
	}
	const uint32_t count = (std::min)(srvCount, desc_.srvCount);
	for (uint32_t i = 0; i < count; ++i)
	{
		commandList->SetGraphicsRootDescriptorTable(kRootParamFirstSrv + i, srvs[i]);
	}
	commandList->DrawInstanced(kFullscreenVertexCount, 1, 0, 0);
}
} // namespace KCE
