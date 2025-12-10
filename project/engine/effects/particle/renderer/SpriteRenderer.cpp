#include "SpriteRenderer.h"
#include <numbers>
#include "effects/particle/ParticleManager.h"
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
}

void SpriteRenderer::Initialize(const std::string& texturePath)
{
	auto* pm = ParticleManager::GetInstance();
	auto* dxCommon = pm->GetDxCommon();

	TextureManager::GetInstance()->LoadTexture(texturePath);
	textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(texturePath);

	materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->uvTransform = MakeIdentity4x4();
	materialData_->enableLighting = false;

	std::vector<VertexData> vertices = {
		{ {  1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
		{ { -1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
		{ {  1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
		{ { -1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
		{ { -1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
		{ {  1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } }
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

	instancingSrvIndex_ = pm->GetSrvManager()->Allocate();
	pm->GetSrvManager()->CreateSRVforStructuredBuffer(
		instancingSrvIndex_,
		instancingResource_.Get(),
		kMaxParticles,
		sizeof(ParticleGPU)
	);
}

void SpriteRenderer::Update(const std::vector<Particle>& particles, CameraManager* camera)
{
	if (particles.empty()) return;

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

void SpriteRenderer::Draw(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	if (instanceCount_ == 0) return;

	auto* cmdList = dxCommon->GetCommandList();
	cmdList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	cmdList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	cmdList->SetGraphicsRootDescriptorTable(1, srvManager->GetGPUDescriptorHandle(instancingSrvIndex_));
	cmdList->SetGraphicsRootDescriptorTable(2, srvManager->GetGPUDescriptorHandle(textureIndex_));
	cmdList->DrawInstanced(6, instanceCount_, 0, 0);
}

void SpriteRenderer::UpdateInstanceData(const Particle& particle, const Matrix4x4& billboardMatrix, CameraManager* camera)
{
	Matrix4x4 scaleMatrix = MakeScaleMatrix(particle.scale);
	Matrix4x4 translateMatrix = MakeTranslateMatrix(particle.position);

	Matrix4x4 worldMatrix = scaleMatrix;
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
