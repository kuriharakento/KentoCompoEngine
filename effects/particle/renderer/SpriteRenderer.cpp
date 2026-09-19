#include "SpriteRenderer.h"
#include <numbers>
#include "effects/particle/ParticleManager.h"
#include "manager/effect/ParticlePipelineManager.h"
#include "manager/scene/CameraManager.h"
#include "manager/system/SrvManager.h"
#include "manager/graphics/TextureManager.h"
#include "base/DirectXCommon.h"
#include "base/GraphicsTypes.h"
#include "base/Logger.h"
#include "effects/particle/diagnostics/ParticleDiagnostics.h"

namespace KCE
{
SpriteRenderer::~SpriteRenderer()
{
	if (instancingResource_)
	{
		instancingResource_->Unmap(0, nullptr);
		instancingResource_.Reset();
	}
	if (materialResource_)
	{
		materialResource_->Unmap(0, nullptr);
		materialResource_.Reset();
	}
	vertexResource_.Reset();

	// SRV解放
	if (instancingSrvIndex_ != SrvManager::kInvalidSrvIndex)
	{
		ParticleManager::GetInstance()->GetSrvManager()->Free(instancingSrvIndex_);
	}
}

void SpriteRenderer::Initialize(const std::string& texturePath)
{
	// デフォルトで白テクスチャをロード
	// 引数がある場合はそれをロード
	std::string path = texturePath.empty() ? "./Resources/uvChecker.png" : texturePath;
	texturePath_ = path;
	TextureManager::GetInstance()->LoadTexture(path);
	TextureManager::GetInstance()->TryGetTextureIndexByFilePath(path, textureIndex_);
	SetEmissiveTexture("");

	auto* pm = ParticleManager::GetInstance();
	InitializeBuffers(pm->GetDxCommon(), pm->GetSrvManager());
}


void SpriteRenderer::InitializeBuffers(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	D3D12_INDIRECT_ARGUMENT_DESC indirectArg{};
	indirectArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
	D3D12_COMMAND_SIGNATURE_DESC signatureDesc{};
	signatureDesc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
	signatureDesc.NumArgumentDescs = 1;
	signatureDesc.pArgumentDescs = &indirectArg;
	dxCommon->GetDevice()->CreateCommandSignature(&signatureDesc, nullptr, IID_PPV_ARGS(&drawCommandSignature_));
	materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->uvTransform = MakeIdentity4x4();
	materialData_->enableLighting = false;

	// TRIANGLE_STRIP用の4頂点（順序: 左上, 右上, 左下, 右下）
	std::vector<VertexData> vertices = {
		{ { -1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }, // 左上
		{ {  1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }, // 右上
		{ { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } }, // 左下
		{ {  1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } }, // 右下
	};

	vertexResource_ = dxCommon->CreateBufferResource(sizeof(VertexData) * vertices.size());
	void* vertData = nullptr;
	vertexResource_->Map(0, nullptr, &vertData);
	std::memcpy(vertData, vertices.data(), sizeof(VertexData) * vertices.size());
	vertexResource_->Unmap(0, nullptr);

	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
	vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * vertices.size());

	instancingResource_ = dxCommon->CreateBufferResource(sizeof(ParticleGPU) * kMaxParticles);
	instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&instancingData_));

	if (!srvManager->TryAllocate(instancingSrvIndex_))
	{
		Logger::Log("SpriteRenderer initialization failed: descriptor heap exhausted\n", Logger::LogLevel::Error);
		return;
	}
	srvManager->CreateSRVforStructuredBuffer(
		instancingSrvIndex_,
		instancingResource_.Get(),
		kMaxParticles,
		sizeof(ParticleGPU)
	);
}

void SpriteRenderer::Update(const std::vector<Particle>& particles, CameraManager* camera)
{
	ParticleScopeTimer timer(ParticleProfileScope::RendererUpdatePerCall);

	if (isGPUMode_)
	{
		instanceCount_ = 0;
		return;
	}

	if (particles.empty())
	{
		instanceCount_ = 0;
		return;
	}

	Matrix4x4 backToFrontMatrix = MakeRotateYMatrix(std::numbers::pi_v<float>);
	Matrix4x4 cameraRotationMatrix = camera->GetActiveCamera()->GetWorldMatrix();
	cameraRotationMatrix.m[3][0] = 0.0f;
	cameraRotationMatrix.m[3][1] = 0.0f;
	cameraRotationMatrix.m[3][2] = 0.0f;
	Matrix4x4 billboardMatrix = backToFrontMatrix * cameraRotationMatrix;

	instanceCount_ = 0;
	for (const auto& particle : particles)
	{
		if (instanceCount_ >= kMaxParticles) break;
		if (!particle.IsAlive()) continue;

		UpdateInstanceData(particle, billboardMatrix, camera);
		++instanceCount_;
	}
}

void SpriteRenderer::SetTexture(const std::string& texturePath)
{
	auto* textures = TextureManager::GetInstance();
	textures->LoadTexture(texturePath);
	uint32_t newIndex = SrvManager::kInvalidSrvIndex;
	if (!textures->TryGetTextureIndexByFilePath(texturePath, newIndex)) return;
	texturePath_ = texturePath;
	textureIndex_ = newIndex;
}

void SpriteRenderer::SetEmissiveTexture(const std::string& texturePath)
{
	emissiveTexturePath_ = texturePath;
	// Emissive texture未指定時は、全経路で共有する無発光テクスチャを使用する。
	const std::string fallback = "./Resources/textures/emissive_black_1x1.png";
	const std::string path = emissiveTexturePath_.empty() || !TextureManager::GetInstance()->CheckTextureExists(emissiveTexturePath_) ? fallback : emissiveTexturePath_;
	TextureManager::GetInstance()->LoadTextureLinear(path);
	emissiveTextureIndex_ = TextureManager::GetInstance()->GetLinearTextureIndexByFilePath(path);
}

void SpriteRenderer::Draw(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	const uint32_t particleSrvIndex = isGPUMode_ ? gpuSrvIndex_ : instancingSrvIndex_;
	if (!srvManager || !srvManager->IsAllocated(particleSrvIndex) || !srvManager->IsAllocated(textureIndex_)) return;
	ParticleScopeTimer timer(ParticleProfileScope::RendererDrawRecordingPerCall);

	if (materialData_)
	{
		materialData_->color = tintColor_;
		materialData_->emissiveColorIntensity = { emissiveSettings_.color.x, emissiveSettings_.color.y, emissiveSettings_.color.z, emissiveSettings_.intensity };
		materialData_->emissiveEnabled = emissiveSettings_.enabled ? 1u : 0u;
		materialData_->emissiveSource = static_cast<uint32_t>(emissiveSettings_.source);
		materialData_->bloomContribution = emissiveSettings_.bloomContribution;
	}

	uint32_t drawCount = isGPUMode_ ? gpuParticleCount_ : instanceCount_;
	if (drawCount == 0) return;

	// コマンドリスト取得
	auto commandList = dxCommon->GetCommandList();
	auto pipelineManager = ParticleManager::GetInstance()->GetPipelineManager();

	// パイプライン設定
	commandList->SetPipelineState(pipelineManager->GetPipelineState(blendMode_, emissiveSettings_.enabled));
	commandList->SetGraphicsRootSignature(pipelineManager->GetRootSignature());

	// プリミティブトポロジ設定
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// 頂点バッファ設定 (SpriteRendererはVB不要だが、一応セットするならここ)
	D3D12_VERTEX_BUFFER_VIEW vbView{};
	vbView.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vbView.SizeInBytes = sizeof(VertexData) * 4;
	vbView.StrideInBytes = sizeof(VertexData);
    
    commandList->IASetVertexBuffers(0, 1, &vbView);

	// マテリアル (Slot 0 - CBV)
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

	// テクスチャ (Slot 2)
	commandList->SetGraphicsRootDescriptorTable(2, srvManager->GetGPUDescriptorHandle(textureIndex_));
	commandList->SetGraphicsRootDescriptorTable(4, srvManager->GetGPUDescriptorHandle(emissiveTextureIndex_));

	// インスタンシングデータ (Slot 1 - VertexShader用)
	commandList->SetGraphicsRootDescriptorTable(1, srvManager->GetGPUDescriptorHandle(particleSrvIndex));

	// 描画
	if (isGPUMode_ && gpuDrawArguments_ && drawCommandSignature_)
	{
		commandList->ExecuteIndirect(drawCommandSignature_.Get(), 1, gpuDrawArguments_, 0, nullptr, 0);
	}
	else
	{
		commandList->DrawInstanced(4, drawCount, 0, 0);
	}
}

void SpriteRenderer::SetGPUMode(bool enable, uint32_t srvIndex, uint32_t count, ID3D12Resource* drawArguments)
{
	isGPUMode_ = enable;
	gpuSrvIndex_ = srvIndex;
	gpuParticleCount_ = count;
	gpuDrawArguments_ = drawArguments;
}

void SpriteRenderer::UpdateInstanceData(const Particle& particle, const Matrix4x4& billboardMatrix, CameraManager* camera)
{
	Matrix4x4 scaleMatrix = MakeScaleMatrix(particle.scale);
	Matrix4x4 rotZMatrix = MakeRotateZMatrix(particle.rotation.z);
	Matrix4x4 translateMatrix = MakeTranslateMatrix(particle.position);

	Matrix4x4 worldMatrix = scaleMatrix * rotZMatrix;
	if (isBillboard_)
	{
		worldMatrix = worldMatrix * billboardMatrix;
	}
	worldMatrix = worldMatrix * translateMatrix;

	Matrix4x4 wvp = Multiply(worldMatrix,
		Multiply(camera->GetActiveCamera()->GetViewMatrix(),
			camera->GetActiveCamera()->GetProjectionMatrix()));

	if (instancingData_)
	{
		instancingData_[instanceCount_].world = worldMatrix;
		instancingData_[instanceCount_].worldViewProj = wvp;
		instancingData_[instanceCount_].color = particle.color;
	}
}
} // namespace KCE
