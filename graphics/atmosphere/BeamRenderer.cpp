#include "graphics/atmosphere/BeamRenderer.h"

#include <cmath>
#include <cstring>
#include <vector>

#include "DirectXTex/d3dx12.h"
#include "base/Camera.h"
#include "base/DirectXCommon.h"
#include "base/Logger.h"
#include "graphics/FrameConstantAllocator.h"
#include "graphics/RenderFormats.h"
#include "graphics/deferred/GBuffer.h"
#include "manager/scene/LightManager.h"
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
/** @brief 円錐の周方向の分割数。ビームは細いので多すぎる必要はない */
constexpr int kConeSegments = 32;
/** @brief 円周率 */
constexpr float kPi = 3.14159265358979323846f;

/**
 * @brief 方向ベクトルに直交する2本の軸を作る
 * @details 円錐の断面の向きを決めるため。どちらを向いていても破綻しないよう、
 *          方向とほぼ平行にならない方の基準軸を選ぶ。
 */
void MakeOrthonormalBasis(const Vector3& direction, Vector3& outX, Vector3& outY)
{
	const Vector3 reference = (std::abs(direction.y) < 0.99f) ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 1.0f, 0.0f, 0.0f };
	outX = Vector3::Cross(reference, direction).Normalize();
	outY = Vector3::Cross(direction, outX).Normalize();
}
} // namespace

BeamRenderer::~BeamRenderer()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->Unregister(this);
	}
#endif
}

void BeamRenderer::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	CreateConeMesh();

	std::string error;
	if (!CreateRootSignature(error) || !CreatePipeline(error))
	{
		// 失敗してもアプリは止めない。ビームが描かれないだけにする
		Logger::Log("BeamRenderer: 初期化に失敗しました\n" + error + "\n", Logger::LogLevel::Error);
	}
}

bool BeamRenderer::IsBeamEnabled(const std::string& lightName) const
{
	const auto it = beamEnabled_.find(lightName);
	return it != beamEnabled_.end() && it->second;
}

float BeamRenderer::GetBeamScale(const std::string& lightName) const
{
	const auto it = beamScale_.find(lightName);
	return it != beamScale_.end() ? it->second : 1.0f;
}

void BeamRenderer::CreateConeMesh()
{
	// 側面だけの円錐。底面は光の出口なので塞がない。
	// カリングしないので、手前と奥の両面が加算され、中心ほど濃く見える
	std::vector<Vector3> vertices;
	vertices.reserve(kConeSegments * 3);

	for (int i = 0; i < kConeSegments; ++i)
	{
		const float angle0 = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(kConeSegments);
		const float angle1 = 2.0f * kPi * static_cast<float>(i + 1) / static_cast<float>(kConeSegments);

		vertices.push_back({ 0.0f, 0.0f, 0.0f });
		vertices.push_back({ std::cos(angle0), std::sin(angle0), 1.0f });
		vertices.push_back({ std::cos(angle1), std::sin(angle1), 1.0f });
	}

	vertexCount_ = static_cast<UINT>(vertices.size());
	const UINT sizeInBytes = static_cast<UINT>(sizeof(Vector3) * vertices.size());

	vertexBuffer_ = dxCommon_->CreateBufferResource(sizeInBytes);
	Vector3* mapped = nullptr;
	vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
	std::memcpy(mapped, vertices.data(), sizeInBytes);
	vertexBuffer_->Unmap(0, nullptr);

	vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeInBytes;
	vertexBufferView_.StrideInBytes = sizeof(Vector3);
}

bool BeamRenderer::CreateRootSignature(std::string& outError)
{
	CD3DX12_DESCRIPTOR_RANGE depthRange{};
	depthRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER rootParams[2]{};
	rootParams[kRootParamConstants].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);
	rootParams[kRootParamDepth].InitAsDescriptorTable(1, &depthRange, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	CD3DX12_ROOT_SIGNATURE_DESC desc{};
	desc.Init(_countof(rootParams), rootParams, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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

bool BeamRenderer::CreatePipeline(std::string& outError)
{
	if (!rootSignature_)
	{
		outError = "ルートシグネチャがありません";
		return false;
	}

	auto vs = dxCommon_->TryCompileShader(L"Resources/shaders/Beam.VS.hlsl", L"vs_6_0", &outError);
	if (!vs) { return false; }
	auto ps = dxCommon_->TryCompileShader(L"Resources/shaders/Beam.PS.hlsl", L"ps_6_0", &outError);
	if (!ps) { return false; }

	D3D12_INPUT_ELEMENT_DESC inputElements[1] = {};
	inputElements[0].SemanticName = "POSITION";
	inputElements[0].SemanticIndex = 0;
	inputElements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature_.Get();
	desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	desc.InputLayout = { inputElements, _countof(inputElements) };
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = kSceneColorFormat;
	desc.SampleDesc.Count = 1;
	desc.SampleMask = UINT_MAX;
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	// 円錐の内側に入っても見えるよう、両面を描く
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	// 深度はシェーダーで比較してフェードさせるため、ハードウェアの深度テストは使わない
	desc.DepthStencilState.DepthEnable = FALSE;
	desc.DepthStencilState.StencilEnable = FALSE;
	desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

	// 光は重なるほど明るくなるので加算。描画順に依存しない
	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	auto& blend = desc.BlendState.RenderTarget[0];
	blend.BlendEnable = TRUE;
	blend.SrcBlend = D3D12_BLEND_ONE;
	blend.DestBlend = D3D12_BLEND_ONE;
	blend.BlendOp = D3D12_BLEND_OP_ADD;
	blend.SrcBlendAlpha = D3D12_BLEND_ZERO;
	blend.DestBlendAlpha = D3D12_BLEND_ONE;
	blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline;
	if (FAILED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline))))
	{
		outError = "ビームのパイプラインの生成に失敗";
		return false;
	}

	pipelineState_ = pipeline;
	return true;
}

bool BeamRenderer::ReloadShaders(std::string& outError)
{
	return CreatePipeline(outError);
}

void BeamRenderer::Draw(Camera* camera, GBuffer* gBuffer, D3D12_CPU_DESCRIPTOR_HANDLE sceneColorRtv,
	LightManager* lightManager, FrameConstantAllocator* allocator)
{
	if (!settings_.enabled || !pipelineState_ || !camera || !gBuffer || !lightManager || !allocator)
	{
		return;
	}

	// 描くビームが1本も無ければ、深度の状態遷移もしない
	bool hasBeam = false;
	for (const auto& [name, light] : lightManager->GetSpotLights())
	{
		(void)light;
		if (IsBeamEnabled(name)) { hasBeam = true; break; }
	}
	if (!hasBeam)
	{
		return;
	}

	auto* commandList = dxCommon_->GetCommandList();

	// 深度を読むために SRV へ切り替え、深度はバインドしない
	gBuffer->TransitionDepthToSRV();
	commandList->OMSetRenderTargets(1, &sceneColorRtv, FALSE, nullptr);

	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->SetGraphicsRootDescriptorTable(kRootParamDepth, srvManager_->GetGPUDescriptorHandle(gBuffer->GetDepthSRVIndex()));

	const Matrix4x4 viewProjection = camera->GetViewProjectionMatrix();
	const Matrix4x4 view = camera->GetViewMatrix();
	const Matrix4x4 invProjection = Inverse(camera->GetProjectionMatrix());
	const Vector2 screenSize = { static_cast<float>(gBuffer->GetWidth()), static_cast<float>(gBuffer->GetHeight()) };

	for (const auto& [name, light] : lightManager->GetSpotLights())
	{
		if (!IsBeamEnabled(name))
		{
			continue;
		}

		const auto& gpu = light.gpuData;
		const float length = gpu.distance * settings_.lengthScale;
		if (length <= 0.0f)
		{
			continue;
		}

		// コーンの半角から、底面（光の届く端）での半径を求める
		float cosAngle = gpu.cosAngle;
		cosAngle = cosAngle < 0.01f ? 0.01f : (cosAngle > 0.9999f ? 0.9999f : cosAngle);
		const float halfAngle = std::acos(cosAngle);
		const float radius = length * std::tan(halfAngle);

		auto allocation = allocator->Allocate(sizeof(ConstantsForGPU));
		if (!allocation.cpuAddress)
		{
			break;
		}

		ConstantsForGPU constants{};
		constants.viewProjection = viewProjection;
		constants.view = view;
		constants.invProjection = invProjection;
		constants.apex = gpu.position;
		constants.length = length;
		constants.direction = gpu.direction.Normalize();
		MakeOrthonormalBasis(constants.direction, constants.axisX, constants.axisY);
		constants.radius = radius;
		constants.intensity = gpu.intensity * settings_.intensity * GetBeamScale(name);
		constants.fadeDistance = settings_.fadeDistance;
		constants.color = { gpu.color.x, gpu.color.y, gpu.color.z };
		constants.edgePower = settings_.edgePower;
		constants.screenSize = screenSize;
		constants.lengthFalloff = settings_.lengthFalloff;
		*static_cast<ConstantsForGPU*>(allocation.cpuAddress) = constants;

		commandList->SetGraphicsRootConstantBufferView(kRootParamConstants, allocation.gpuAddress);
		commandList->DrawInstanced(vertexCount_, 1, 0, 0);
	}

	// 後続のパスは深度を書き込み用として使うので戻す
	gBuffer->TransitionDepthToDepthWrite();
}

#ifdef USE_IMGUI
void BeamRenderer::RegisterDebugUI(LightManager* lightManager)
{
	debugLightManager_ = lightManager;
	DebugUIManager::GetInstance()->RegisterSettingsPage(this, "Rendering", "Light Beams", [this]() { DrawImGui(); });
}

void BeamRenderer::DrawImGui()
{
	ImGui::Checkbox("Enabled", &settings_.enabled);
	ImGui::DragFloat("Intensity", &settings_.intensity, 0.005f, 0.0f, 10.0f, "%.3f");
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("ビーム全体の明るさ。空気の濃さに相当する"); }
	ImGui::DragFloat("Length Falloff", &settings_.lengthFalloff, 0.05f, 0.0f, 10.0f, "%.2f");
	ImGui::DragFloat("Edge Power", &settings_.edgePower, 0.05f, 0.1f, 16.0f, "%.2f");
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("大きいほど筋の縁が暗くなり、細く見える"); }
	ImGui::DragFloat("Fade Distance", &settings_.fadeDistance, 0.05f, 0.01f, 20.0f, "%.2f");
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("壁や床の手前で消えていく距離。小さいと床に刺さった線が見える"); }
	ImGui::DragFloat("Length Scale", &settings_.lengthScale, 0.01f, 0.05f, 4.0f, "%.2f");

	ImGui::SeparatorText("スポットライトごとの有効／無効");
	if (!debugLightManager_ || debugLightManager_->GetSpotLights().empty())
	{
		ImGui::TextDisabled("スポットライトがありません");
		return;
	}

	if (ImGui::Button("All On"))
	{
		for (const auto& [name, light] : debugLightManager_->GetSpotLights()) { (void)light; beamEnabled_[name] = true; }
	}
	ImGui::SameLine();
	if (ImGui::Button("All Off"))
	{
		for (const auto& [name, light] : debugLightManager_->GetSpotLights()) { (void)light; beamEnabled_[name] = false; }
	}

	for (const auto& [name, light] : debugLightManager_->GetSpotLights())
	{
		(void)light;
		ImGui::PushID(name.c_str());
		bool enabled = IsBeamEnabled(name);
		if (ImGui::Checkbox("##enabled", &enabled))
		{
			beamEnabled_[name] = enabled;
		}
		ImGui::SameLine();
		float scale = GetBeamScale(name);
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::DragFloat(name.c_str(), &scale, 0.01f, 0.0f, 10.0f, "x%.2f"))
		{
			beamScale_[name] = scale;
		}
		ImGui::PopID();
	}
}
#endif
} // namespace KCE
