#include "graphics/text/Text3DRenderer.h"

#include <algorithm>
#include <cstring>

#include "DirectXTex/d3dx12.h"
#include "base/Camera.h"
#include "base/DirectXCommon.h"
#include "base/Logger.h"
#include "graphics/FrameConstantAllocator.h"
#include "graphics/RenderFormats.h"
#include "graphics/2d/GlyphAtlas.h"
#include "manager/graphics/TextureManager.h"

#ifdef USE_IMGUI
#include <cstdio>
#include "externals/imgui/imgui.h"
#include "editor/SelectionContext.h"
#include "manager/editor/DebugUIManager.h"
#endif

namespace KCE
{
namespace
{
constexpr UINT kRootParamCamera = 0;
constexpr UINT kRootParamInstances = 1;
constexpr UINT kRootParamAtlas = 2;
constexpr UINT kVerticesPerGlyph = 6;
// シーンの深度と同じフォーマット（G-Buffer / フォワードと共通）
constexpr DXGI_FORMAT kDepthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
#ifdef USE_IMGUI
constexpr size_t kTextBufferSize = 512;
constexpr float kDragSpeed = 0.05f;
constexpr float kTextBoxHeight = 60.0f;
#endif
} // namespace

Text3DRenderer::~Text3DRenderer()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->Unregister(this);
	}
#endif
}

void Text3DRenderer::Initialize(DirectXCommon* dxCommon, GlyphAtlas* atlas)
{
	dxCommon_ = dxCommon;
	atlas_ = atlas;
	instances_.reserve(kMaxGlyphs);

	std::string error;
	if (!CreateRootSignature(error) || !CreatePipeline(error))
	{
		Logger::Log("Text3DRenderer: 初期化に失敗しました\n" + error + "\n", Logger::LogLevel::Error);
	}
}

void Text3DRenderer::Register(const std::string& name, TextMesh3D* mesh)
{
	if (mesh)
	{
		meshes_[name] = mesh;
	}
}

void Text3DRenderer::Unregister(TextMesh3D* mesh)
{
	for (auto it = meshes_.begin(); it != meshes_.end();)
	{
		it = (it->second == mesh) ? meshes_.erase(it) : std::next(it);
	}
}

TextMesh3D* Text3DRenderer::Find(const std::string& name) const
{
	const auto it = meshes_.find(name);
	return it != meshes_.end() ? it->second : nullptr;
}

bool Text3DRenderer::CreateRootSignature(std::string& outError)
{
	CD3DX12_DESCRIPTOR_RANGE atlasRange{};
	atlasRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

	CD3DX12_ROOT_PARAMETER params[3]{};
	params[kRootParamCamera].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
	// 板のデータはフレームの定数置き場に書いて、StructuredBuffer として直接読む
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

bool Text3DRenderer::CreatePipeline(std::string& outError)
{
	if (!rootSignature_)
	{
		outError = "ルートシグネチャがありません";
		return false;
	}
	auto vs = dxCommon_->TryCompileShader(L"Resources/shaders/Text3D.VS.hlsl", L"vs_6_0", &outError);
	if (!vs) { return false; }
	auto ps = dxCommon_->TryCompileShader(L"Resources/shaders/Text3D.PS.hlsl", L"ps_6_0", &outError);
	if (!ps) { return false; }
	auto depthPs = dxCommon_->TryCompileShader(L"Resources/shaders/Text3DDepth.PS.hlsl", L"ps_6_0", &outError);
	if (!depthPs) { return false; }

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature_.Get();
	desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	desc.InputLayout = { nullptr, 0 };
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = kSceneColorFormat;
	desc.DSVFormat = kDepthFormat;
	desc.SampleDesc.Count = 1;
	desc.SampleMask = UINT_MAX;
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	// 回りながら出てくる文字は裏も見える
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	// 物の奥には隠れるが、文字どうしは重ねる
	desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	auto& blend = desc.BlendState.RenderTarget[0];
	blend.BlendEnable = TRUE;
	blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blend.BlendOp = D3D12_BLEND_OP_ADD;
	blend.SrcBlendAlpha = D3D12_BLEND_ZERO;
	blend.DestBlendAlpha = D3D12_BLEND_ONE;
	blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;

	// 深度だけのパス。色は書かず、濃い所の深度を書く
	D3D12_GRAPHICS_PIPELINE_STATE_DESC depthDesc = desc;
	depthDesc.PS = { depthPs->GetBufferPointer(), depthPs->GetBufferSize() };
	depthDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	depthDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> depthPipeline;
	if (FAILED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline)))
		|| FAILED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&depthDesc, IID_PPV_ARGS(&depthPipeline))))
	{
		outError = "3D 文字のパイプラインの生成に失敗";
		return false;
	}
	pipelineState_ = pipeline;
	depthPipelineState_ = depthPipeline;
	return true;
}

void Text3DRenderer::Draw(Camera* camera, D3D12_CPU_DESCRIPTOR_HANDLE sceneColorRtv, D3D12_CPU_DESCRIPTOR_HANDLE depthDsv, FrameConstantAllocator* allocator)
{
	if (!pipelineState_ || !camera || !allocator || !atlas_ || !atlas_->IsReady() || meshes_.empty())
	{
		return;
	}

	instances_.clear();
	for (const auto& [name, mesh] : meshes_)
	{
		mesh->BuildInstances(*atlas_, instances_);
	}
	if (instances_.empty())
	{
		return;
	}
	const size_t count = (std::min)(instances_.size(), kMaxGlyphs);

	auto cameraAllocation = allocator->Allocate(sizeof(Matrix4x4));
	auto instanceAllocation = allocator->Allocate(sizeof(TextMesh3D::Instance) * count);
	if (!cameraAllocation.cpuAddress || !instanceAllocation.cpuAddress)
	{
		return;
	}
	*static_cast<Matrix4x4*>(cameraAllocation.cpuAddress) = camera->GetViewProjectionMatrix();
	std::memcpy(instanceAllocation.cpuAddress, instances_.data(), sizeof(TextMesh3D::Instance) * count);

	// 初めて使う文字はいま描き足されたので、描く前に送る
	atlas_->FlushUploads();

	auto* commandList = dxCommon_->GetCommandList();
	commandList->OMSetRenderTargets(1, &sceneColorRtv, FALSE, &depthDsv);
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootConstantBufferView(kRootParamCamera, cameraAllocation.gpuAddress);
	commandList->SetGraphicsRootShaderResourceView(kRootParamInstances, instanceAllocation.gpuAddress);
	commandList->SetGraphicsRootDescriptorTable(kRootParamAtlas, TextureManager::GetInstance()->GetSrvHandleGPU(GlyphAtlas::kTextureKey));

	// 先に濃い所の深度を書いてから、同じ板に色を重ねる（縁のアンチエイリアスは色のパスで残る）
	commandList->SetPipelineState(depthPipelineState_.Get());
	commandList->DrawInstanced(kVerticesPerGlyph, static_cast<UINT>(count), 0, 0);
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->DrawInstanced(kVerticesPerGlyph, static_cast<UINT>(count), 0, 0);
}

#ifdef USE_IMGUI
void Text3DRenderer::RegisterDebugUI()
{
	DebugUIManager::GetInstance()->RegisterHierarchySection(this, "3D Text", [this]() { DrawHierarchyImGui(); });
	DebugUIManager::GetInstance()->RegisterInspector(this, SelectionKind::Text3D,
		[this](const SelectionItem& item) { DrawInspectorImGui(item); });
}

void Text3DRenderer::DrawHierarchyImGui()
{
	if (meshes_.empty())
	{
		ImGui::TextDisabled("登録されている 3D 文字はない。");
		return;
	}
	const SelectionItem& primary = SelectionContext::GetInstance()->GetPrimary();
	for (const auto& [name, mesh] : meshes_)
	{
		const bool isSelected = primary.kind == SelectionKind::Text3D && primary.name == name;
		if (ImGui::Selectable(name.c_str(), isSelected))
		{
			SelectionItem item;
			item.kind = SelectionKind::Text3D;
			item.name = name;
			SelectionContext::GetInstance()->Select(item);
		}
	}
}

void Text3DRenderer::DrawInspectorImGui(const SelectionItem& item)
{
	TextMesh3D* mesh = Find(item.name);
	if (!mesh)
	{
		ImGui::TextDisabled("3D 文字が見つからない。");
		return;
	}

	char buffer[kTextBufferSize];
	std::snprintf(buffer, sizeof(buffer), "%s", mesh->GetText().c_str());
	if (ImGui::InputTextMultiline("Text", buffer, sizeof(buffer), ImVec2(0.0f, kTextBoxHeight)))
	{
		mesh->SetText(buffer);
	}
	TextMesh3D::Params& params = mesh->GetParams();
	ImGui::DragFloat3("Position", &params.position.x, kDragSpeed);
	ImGui::DragFloat("Size", &params.size, kDragSpeed * 0.1f, 0.01f, 100.0f);
	ImGui::ColorEdit4("Color", &params.color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
	const float charCount = static_cast<float>(mesh->GetCharCount());
	float reveal = (std::min)(params.reveal, charCount);
	if (ImGui::SliderFloat("Reveal", &reveal, 0.0f, charCount, "%.2f"))
	{
		params.reveal = reveal;
	}
	ImGui::SliderFloat("Exit", &params.exit, 0.0f, charCount, "%.2f");
	int style = static_cast<int>(params.style);
	if (ImGui::Combo("Style", &style, "Fade\0Drop\0Spin\0Pop\0"))
	{
		params.style = static_cast<TextAppearStyle>(style);
	}
}
#endif
} // namespace KCE
