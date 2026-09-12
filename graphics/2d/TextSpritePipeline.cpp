#include "graphics/2d/TextSpritePipeline.h"

#include <cstring>

#include "DirectXTex/d3dx12.h"
#include "base/DirectXCommon.h"
#include "base/Logger.h"
#include "graphics/FrameConstantAllocator.h"
#include "graphics/RenderFormats.h"
#include "graphics/2d/GlyphAtlas.h"
#include "manager/graphics/TextureManager.h"

namespace KCE
{
namespace
{
constexpr UINT kRootParamInstances = 0;
constexpr UINT kRootParamAtlas = 1;
constexpr UINT kVerticesPerGlyph = 6;
// 2D の描画先に一緒に束ねられている深度のフォーマット（Sprite と同じ）。深度は使わない
constexpr DXGI_FORMAT kDepthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
} // namespace

TextSpritePipeline::~TextSpritePipeline() = default;

bool TextSpritePipeline::Initialize(DirectXCommon* dxCommon, FrameConstantAllocator* allocator)
{
	dxCommon_ = dxCommon;
	allocator_ = allocator;
	std::string error;
	if (!CreateRootSignature(error) || !CreatePipeline(error))
	{
		Logger::Log("TextSpritePipeline: 初期化に失敗しました\n" + error + "\n", Logger::LogLevel::Error);
		return false;
	}
	return true;
}

bool TextSpritePipeline::CreateRootSignature(std::string& outError)
{
	CD3DX12_DESCRIPTOR_RANGE atlasRange{};
	atlasRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

	CD3DX12_ROOT_PARAMETER params[2]{};
	params[kRootParamInstances].InitAsShaderResourceView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
	params[kRootParamAtlas].InitAsDescriptorTable(1, &atlasRange, D3D12_SHADER_VISIBILITY_PIXEL);

	const CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		0.0f, 16, D3D12_COMPARISON_FUNC_NEVER, D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK, 0.0f, D3D12_FLOAT32_MAX, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_ROOT_SIGNATURE_DESC desc{};
	desc.Init(_countof(params), params, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE);

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

bool TextSpritePipeline::CreatePipeline(std::string& outError)
{
	if (!rootSignature_)
	{
		outError = "ルートシグネチャがありません";
		return false;
	}
	auto vs = dxCommon_->TryCompileShader(L"Resources/shaders/TextSprite.VS.hlsl", L"vs_6_0", &outError);
	if (!vs) { return false; }
	auto ps = dxCommon_->TryCompileShader(L"Resources/shaders/TextSprite.PS.hlsl", L"ps_6_0", &outError);
	if (!ps) { return false; }

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature_.Get();
	desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	desc.InputLayout = { nullptr, 0 };
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = kDisplayColorFormat;
	desc.DSVFormat = kDepthFormat;
	desc.SampleDesc.Count = 1;
	desc.SampleMask = UINT_MAX;
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	// UI なので深度は見ない（後に描いた物が手前）
	desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	desc.DepthStencilState.DepthEnable = FALSE;

	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	auto& blend = desc.BlendState.RenderTarget[0];
	blend.BlendEnable = TRUE;
	blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blend.BlendOp = D3D12_BLEND_OP_ADD;
	blend.SrcBlendAlpha = D3D12_BLEND_ONE;
	blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline;
	if (FAILED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline))))
	{
		outError = "2D 文字のパイプラインの生成に失敗";
		return false;
	}
	pipelineState_ = pipeline;
	return true;
}

void TextSpritePipeline::Draw(const Instance* instances, size_t count)
{
	if (!pipelineState_ || !allocator_ || !instances || count == 0)
	{
		return;
	}
	auto allocation = allocator_->Allocate(sizeof(Instance) * count);
	if (!allocation.cpuAddress)
	{
		return;
	}
	std::memcpy(allocation.cpuAddress, instances, sizeof(Instance) * count);

	auto* commandList = dxCommon_->GetCommandList();
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootShaderResourceView(kRootParamInstances, allocation.gpuAddress);
	commandList->SetGraphicsRootDescriptorTable(kRootParamAtlas, TextureManager::GetInstance()->GetSrvHandleGPU(GlyphAtlas::kTextureKey));
	commandList->DrawInstanced(kVerticesPerGlyph, static_cast<UINT>(count), 0, 0);
}
} // namespace KCE
