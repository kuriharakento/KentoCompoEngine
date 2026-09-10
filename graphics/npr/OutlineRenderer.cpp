#include "graphics/npr/OutlineRenderer.h"

#include "DirectXTex/d3dx12.h"
#include "base/Camera.h"
#include "base/DirectXCommon.h"
#include "base/Logger.h"
#include "graphics/FrameConstantAllocator.h"
#include "graphics/RenderFormats.h"
#include "graphics/deferred/GBuffer.h"
#include "manager/system/SrvManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "manager/editor/DebugUIManager.h"
#endif

namespace KCE
{
namespace
{
/** @brief ルートパラメータ: 定数（b0） */
constexpr UINT kRootParamConstants = 0;
/** @brief ルートパラメータ: G-Buffer（t0〜t4。ライトパスと同じ並び） */
constexpr UINT kRootParamGBuffer = 1;
/** @brief G-Buffer の SRV の数（アルベド・法線・材質・発光・深度） */
constexpr UINT kGBufferSrvCount = 5;
} // namespace

OutlineRenderer::~OutlineRenderer()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->UnregisterDebugUI(this);
	}
#endif
}

void OutlineRenderer::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	std::string error;
	if (!CreateRootSignature(error) || !CreatePipeline(error))
	{
		// 失敗してもアプリは止めない。線が描かれないだけにする
		Logger::Log("OutlineRenderer: 初期化に失敗しました\n" + error + "\n", Logger::LogLevel::Error);
	}
}

bool OutlineRenderer::CreateRootSignature(std::string& outError)
{
	// ライトパスと同じく、G-Buffer の5つの SRV を1つのテーブルで読む
	CD3DX12_DESCRIPTOR_RANGE gBufferRange{};
	gBufferRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, kGBufferSrvCount, 0);

	CD3DX12_ROOT_PARAMETER rootParams[2]{};
	rootParams[kRootParamConstants].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[kRootParamGBuffer].InitAsDescriptorTable(1, &gBufferRange, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	CD3DX12_ROOT_SIGNATURE_DESC desc{};
	desc.Init(_countof(rootParams), rootParams, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE);

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

bool OutlineRenderer::CreatePipeline(std::string& outError)
{
	if (!rootSignature_)
	{
		outError = "ルートシグネチャがありません";
		return false;
	}

	auto vs = dxCommon_->TryCompileShader(L"Resources/shaders/PostEffect.VS.hlsl", L"vs_6_0", &outError);
	if (!vs) { return false; }
	auto ps = dxCommon_->TryCompileShader(L"Resources/shaders/Outline.PS.hlsl", L"ps_6_0", &outError);
	if (!ps) { return false; }

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature_.Get();
	desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	desc.InputLayout = { nullptr, 0 };
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = kSceneColorFormat;
	desc.SampleDesc.Count = 1;
	desc.SampleMask = UINT_MAX;
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc.DepthStencilState.DepthEnable = FALSE;
	desc.DepthStencilState.StencilEnable = FALSE;
	desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

	// 線の色を、線の濃さ（アルファ）で重ねる
	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	auto& blend = desc.BlendState.RenderTarget[0];
	blend.BlendEnable = TRUE;
	blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blend.BlendOp = D3D12_BLEND_OP_ADD;
	blend.SrcBlendAlpha = D3D12_BLEND_ZERO;
	blend.DestBlendAlpha = D3D12_BLEND_ONE;
	blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline;
	if (FAILED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline))))
	{
		outError = "アウトラインのパイプラインの生成に失敗";
		return false;
	}

	pipelineState_ = pipeline;
	return true;
}

bool OutlineRenderer::ReloadShaders(std::string& outError)
{
	return CreatePipeline(outError);
}

void OutlineRenderer::Draw(Camera* camera, GBuffer* gBuffer, D3D12_CPU_DESCRIPTOR_HANDLE sceneColorRtv, FrameConstantAllocator* allocator)
{
	if (!settings_.enabled || !pipelineState_ || !camera || !gBuffer || !allocator)
	{
		return;
	}

	auto allocation = allocator->Allocate(sizeof(ConstantsForGPU));
	if (!allocation.cpuAddress)
	{
		return;
	}

	ConstantsForGPU constants{};
	constants.invProjection = Inverse(camera->GetProjectionMatrix());
	constants.screenSize = { static_cast<float>(gBuffer->GetWidth()), static_cast<float>(gBuffer->GetHeight()) };
	constants.width = settings_.width;
	constants.depthThreshold = settings_.depthThreshold;
	constants.color = settings_.color;
	constants.normalThreshold = settings_.normalThreshold;
	*static_cast<ConstantsForGPU*>(allocation.cpuAddress) = constants;

	auto* commandList = dxCommon_->GetCommandList();

	// 深度を読むために SRV へ切り替え、深度はバインドしない
	gBuffer->TransitionDepthToSRV();
	commandList->OMSetRenderTargets(1, &sceneColorRtv, FALSE, nullptr);

	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootConstantBufferView(kRootParamConstants, allocation.gpuAddress);
	commandList->SetGraphicsRootDescriptorTable(kRootParamGBuffer, srvManager_->GetGPUDescriptorHandle(gBuffer->GetSRVIndex(0)));
	commandList->DrawInstanced(3, 1, 0, 0);

	// 後続のパスは深度を書き込み用として使うので戻す
	gBuffer->TransitionDepthToDepthWrite();
}

#ifdef USE_IMGUI
void OutlineRenderer::RegisterDebugUI()
{
	DebugUIManager::GetInstance()->RegisterDebugUI(this, "Outline", [this]() { DrawImGui(); }, DebugUIArea::Inspector);
}

void OutlineRenderer::DrawImGui()
{
	ImGui::TextDisabled("線を引くかは素材ごと（SetOutlineStrength）。既定は 0");
	ImGui::TextDisabled("ディファードで描くオブジェクトにだけ効く");
	ImGui::Separator();
	ImGui::Checkbox("Enabled", &settings_.enabled);
	ImGui::DragFloat("Width", &settings_.width, 0.05f, 0.5f, 8.0f, "%.2f px");
	ImGui::DragFloat("Depth Threshold", &settings_.depthThreshold, 0.002f, 0.001f, 1.0f, "%.3f");
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("奥行きの差がこの割合を超えたら輪郭。小さいほど線が増える"); }
	ImGui::DragFloat("Normal Threshold", &settings_.normalThreshold, 0.01f, -1.0f, 1.0f, "%.2f");
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("面の向きの差でも線を引く（折れ目）。1 に近いほど線が増える"); }
	ImGui::ColorEdit3("Color", &settings_.color.x);
	if (ImGui::Button("Reset"))
	{
		const bool enabled = settings_.enabled;
		settings_ = Settings{};
		settings_.enabled = enabled;
	}
}
#endif
} // namespace KCE
