#include "graphics/atmosphere/FogRenderer.h"

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
/** @brief ルートパラメータ: 深度（t0） */
constexpr UINT kRootParamDepth = 1;
} // namespace

FogRenderer::~FogRenderer()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->Unregister(this);
	}
#endif
}

void FogRenderer::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	std::string error;
	if (!CreateRootSignature(error) || !CreatePipeline(error))
	{
		// 失敗してもアプリは止めない。フォグが描かれないだけにする
		Logger::Log("FogRenderer: 初期化に失敗しました\n" + error + "\n", Logger::LogLevel::Error);
	}
}

bool FogRenderer::CreateRootSignature(std::string& outError)
{
	CD3DX12_DESCRIPTOR_RANGE depthRange{};
	depthRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER rootParams[2]{};
	rootParams[kRootParamConstants].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[kRootParamDepth].InitAsDescriptorTable(1, &depthRange, D3D12_SHADER_VISIBILITY_PIXEL);

	// 深度は補間せずに読みたいのでポイントサンプリング
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

bool FogRenderer::CreatePipeline(std::string& outError)
{
	if (!rootSignature_)
	{
		outError = "ルートシグネチャがありません";
		return false;
	}

	auto vs = dxCommon_->TryCompileShader(L"Resources/shaders/PostEffect.VS.hlsl", L"vs_6_0", &outError);
	if (!vs) { return false; }
	auto ps = dxCommon_->TryCompileShader(L"Resources/shaders/Fog.PS.hlsl", L"ps_6_0", &outError);
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
	// 深度はシェーダーで読むので、深度テスト・書き込みは行わない
	desc.DepthStencilState.DepthEnable = FALSE;
	desc.DepthStencilState.StencilEnable = FALSE;
	desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

	// 霧の色を、霧の濃さ（アルファ）で重ねる
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
		outError = "フォグのパイプラインの生成に失敗";
		return false;
	}

	// ここまで来たら成功しているので差し替える
	pipelineState_ = pipeline;
	return true;
}

bool FogRenderer::ReloadShaders(std::string& outError)
{
	return CreatePipeline(outError);
}

void FogRenderer::Draw(Camera* camera, GBuffer* gBuffer, D3D12_CPU_DESCRIPTOR_HANDLE sceneColorRtv, FrameConstantAllocator* allocator)
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
	constants.invViewProjection = Inverse(camera->GetViewProjectionMatrix());
	constants.cameraPosition = camera->GetTranslate();
	constants.density = settings_.density;
	constants.color = settings_.color;
	constants.heightFalloff = settings_.heightFalloff;
	constants.baseHeight = settings_.baseHeight;
	constants.maxOpacity = settings_.maxOpacity;
	constants.skyAmount = settings_.skyAmount;
	*static_cast<ConstantsForGPU*>(allocation.cpuAddress) = constants;

	auto* commandList = dxCommon_->GetCommandList();

	// 深度を読むために SRV へ切り替え、深度はバインドしない
	gBuffer->TransitionDepthToSRV();
	commandList->OMSetRenderTargets(1, &sceneColorRtv, FALSE, nullptr);

	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootConstantBufferView(kRootParamConstants, allocation.gpuAddress);
	commandList->SetGraphicsRootDescriptorTable(kRootParamDepth, srvManager_->GetGPUDescriptorHandle(gBuffer->GetDepthSRVIndex()));
	commandList->DrawInstanced(3, 1, 0, 0);

	// 後続のパスは深度を書き込み用として使うので戻す
	gBuffer->TransitionDepthToDepthWrite();
}

#ifdef USE_IMGUI
void FogRenderer::RegisterDebugUI()
{
	DebugUIManager::GetInstance()->RegisterSettingsPage(this, "レンダリング", "フォグ", [this]() { DrawImGui(); });
}

void FogRenderer::DrawImGui()
{
	ImGui::Checkbox("有効", &settings_.enabled);
	ImGui::ColorEdit3("色", &settings_.color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
	ImGui::DragFloat("濃さ", &settings_.density, 0.001f, 0.0f, 1.0f, "%.4f");
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("距離あたりの濃さ。大きいほど近くで霞む"); }
	ImGui::DragFloat("基準の高さ", &settings_.baseHeight, 0.1f, -100.0f, 100.0f, "%.2f");
	ImGui::DragFloat("高さでの減衰", &settings_.heightFalloff, 0.005f, 0.0f, 5.0f, "%.3f");
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("基準の高さより上で薄くなる速さ。0 で高さに関係なく一様"); }
	ImGui::SliderFloat("最大の不透明度", &settings_.maxOpacity, 0.0f, 1.0f, "%.2f");
	ImGui::SliderFloat("空への掛かり具合", &settings_.skyAmount, 0.0f, 1.0f, "%.2f");
	if (ImGui::Button("リセット"))
	{
		const bool enabled = settings_.enabled;
		settings_ = Settings{};
		settings_.enabled = enabled;
	}
}
#endif
} // namespace KCE
