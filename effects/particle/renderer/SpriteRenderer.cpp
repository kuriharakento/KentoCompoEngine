#include "SpriteRenderer.h"
#include <numbers>
#include "effects/particle/gpu/GPUParticlePipeline.h"
#include "effects/particle/ParticleManager.h"
#include "manager/effect/ParticlePipelineManager.h"
#include "manager/scene/CameraManager.h"
#include "manager/system/SrvManager.h"
#include "manager/graphics/TextureManager.h"
#include "base/DirectXCommon.h"
#include "base/GraphicsTypes.h"

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
	textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(path);

	auto* pm = ParticleManager::GetInstance();
	InitializeBuffers(pm->GetDxCommon(), pm->GetSrvManager());
}


void SpriteRenderer::InitializeBuffers(DirectXCommon* dxCommon, SrvManager* srvManager)
{
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

	instancingSrvIndex_ = srvManager->Allocate();
	srvManager->CreateSRVforStructuredBuffer(
		instancingSrvIndex_,
		instancingResource_.Get(),
		kMaxParticles,
		sizeof(ParticleGPU)
	);
}

void SpriteRenderer::Update(const std::vector<Particle>& particles, CameraManager* camera)
{
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
	texturePath_ = texturePath;
	TextureManager::GetInstance()->LoadTexture(texturePath);
	textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(texturePath);
}

void SpriteRenderer::Draw(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	uint32_t drawCount = isGPUMode_ ? gpuParticleCount_ : instanceCount_;
	if (drawCount == 0) return;

	// コマンドリスト取得
	auto commandList = dxCommon->GetCommandList();
	auto pipelineManager = ParticleManager::GetInstance()->GetPipelineManager();

	// パイプライン設定
	commandList->SetPipelineState(pipelineManager->GetPipelineState(blendMode_));
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

	// インスタンシングデータ (Slot 1 - VertexShader用)
	uint32_t index = isGPUMode_ ? gpuSrvIndex_ : instancingSrvIndex_;
	commandList->SetGraphicsRootDescriptorTable(1, srvManager->GetGPUDescriptorHandle(index));

	// 描画
	if (isGPUMode_ && gpuIndirectArgsBuffer_)
	{
		commandList->ExecuteIndirect(
			GPUParticlePipeline::GetInstance()->GetCommandSignature(),
			1,
			gpuIndirectArgsBuffer_,
			0,
			nullptr,
			0
		);
	}
	else
	{
		commandList->DrawInstanced(4, drawCount, 0, 0);
	}
}

void SpriteRenderer::SetGPUMode(bool enable, uint32_t srvIndex, uint32_t count, ID3D12Resource* indirectArgsBuffer)
{
	isGPUMode_ = enable;
	gpuSrvIndex_ = srvIndex;
	gpuParticleCount_ = count;
	gpuIndirectArgsBuffer_ = indirectArgsBuffer;
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
