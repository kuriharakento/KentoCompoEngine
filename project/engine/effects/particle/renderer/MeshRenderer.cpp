#include "effects/particle/renderer/MeshRenderer.h"
#include "effects/particle/gpu/GPUParticlePipeline.h"
#include "base/DirectXCommon.h"
#include "base/GraphicsTypes.h"
#include "manager/system/SrvManager.h"
#include "manager/scene/CameraManager.h"
#include "base/Camera.h"
#include "math/MatrixFunc.h"
#include "effects/particle/ParticleManager.h"
#include "manager/graphics/TextureManager.h"
// #include "externals/DirectXTex/d3dx12.h" // Assuming this is where it is, or use local if configured
#include "manager/effect/ParticlePipelineManager.h"
#include <d3d12.h>
#include <numbers>

MeshRenderer::~MeshRenderer()
{
	if (instanceResource_)
	{
		instanceResource_->Unmap(0, nullptr);
	}
	if (instanceSrvIndex_ != 0)
	{
		ParticleManager::GetInstance()->GetSrvManager()->Free(instanceSrvIndex_);
	}
}

void MeshRenderer::SetPrimitive(PrimitiveType type)
{
	primitiveType_ = type;
	needsRebuild_ = true;
}

void MeshRenderer::SetPrimitive(PrimitiveType type, const PrimitiveOptions& options)
{
	primitiveType_ = type;
	options_ = options;
	needsRebuild_ = true;
}

void MeshRenderer::RegeneratePrimitive()
{
	primitiveMesh_ = PrimitiveGenerator::Generate(primitiveType_, options_);
	primitiveVertexResource_.Reset();
	primitiveIndexResource_.Reset();
	needsRebuild_ = false;
}

void MeshRenderer::InitializeBuffers(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	if (instanceResource_) return;

	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeof(ParticleGPU) * kMaxInstances;
	resourceDesc.Height = 1;

	// ... (Skipping unchanged lines 56-59)
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	dxCommon->GetDevice()->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&instanceResource_)
	);

	instanceResource_->Map(0, nullptr, reinterpret_cast<void**>(&instanceData_));

	instanceSrvIndex_ = srvManager->Allocate();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = kMaxInstances;
	srvDesc.Buffer.StructureByteStride = sizeof(ParticleGPU);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	dxCommon->GetDevice()->CreateShaderResourceView(
		instanceResource_.Get(),
		&srvDesc,
		srvManager->GetCPUDescriptorHandle(instanceSrvIndex_)
	);

	// マテリアルリソースの初期化
	materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->uvTransform = MakeIdentity4x4();
	materialData_->enableLighting = false;
}

void MeshRenderer::CreatePrimitiveBuffers(DirectXCommon* dxCommon)
{
	if (needsRebuild_)
	{
		// 既存バッファをリセットして再作成を可能にする
		primitiveVertexResource_.Reset();
		primitiveIndexResource_.Reset();
		RegeneratePrimitive();
	}

	if (primitiveVertexResource_) return;
	if (primitiveMesh_.vertices.empty()) return;

	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	// 頂点バッファ
	{
		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = sizeof(PrimitiveVertex) * primitiveMesh_.vertices.size();
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		dxCommon->GetDevice()->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&primitiveVertexResource_)
		);

		void* data = nullptr;
		primitiveVertexResource_->Map(0, nullptr, &data);
		memcpy(data, primitiveMesh_.vertices.data(), sizeof(PrimitiveVertex) * primitiveMesh_.vertices.size());
		primitiveVertexResource_->Unmap(0, nullptr);

		primitiveVertexView_.BufferLocation = primitiveVertexResource_->GetGPUVirtualAddress();
		primitiveVertexView_.SizeInBytes = static_cast<UINT>(sizeof(PrimitiveVertex) * primitiveMesh_.vertices.size());
		primitiveVertexView_.StrideInBytes = sizeof(PrimitiveVertex);
	}

	// インデックスバッファ
	{
		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = sizeof(uint32_t) * primitiveMesh_.indices.size();
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		dxCommon->GetDevice()->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&primitiveIndexResource_)
		);

		void* data = nullptr;
		primitiveIndexResource_->Map(0, nullptr, &data);
		memcpy(data, primitiveMesh_.indices.data(), sizeof(uint32_t) * primitiveMesh_.indices.size());
		primitiveIndexResource_->Unmap(0, nullptr);

		primitiveIndexView_.BufferLocation = primitiveIndexResource_->GetGPUVirtualAddress();
		primitiveIndexView_.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * primitiveMesh_.indices.size());
		primitiveIndexView_.Format = DXGI_FORMAT_R32_UINT;
	}
}

void MeshRenderer::Update(const std::vector<Particle>& particles, CameraManager* camera)
{
	instanceCount_ = 0;

	Camera* activeCamera = camera->GetActiveCamera();
	if (!activeCamera) return;

	// ビルボード用の回転行列（カメラの逆回転）
	Matrix4x4 billboardMatrix = MakeIdentity4x4();
	if (useBillboard_)
	{
		Matrix4x4 backToFrontMatrix = MakeRotateYMatrix(std::numbers::pi_v<float>);
		Matrix4x4 cameraRotationMatrix = activeCamera->GetWorldMatrix();
		cameraRotationMatrix.m[3][0] = 0.0f;
		cameraRotationMatrix.m[3][1] = 0.0f;
		cameraRotationMatrix.m[3][2] = 0.0f;
		billboardMatrix = Multiply(backToFrontMatrix, cameraRotationMatrix);
	}

	for (const auto& particle : particles)
	{
		if (!particle.IsAlive()) continue;
		if (instanceCount_ >= kMaxInstances) break;

		Matrix4x4 scaleMatrix = MakeScaleMatrix({
			particle.scale.x * baseScale_,
			particle.scale.y * baseScale_,
			particle.scale.z * baseScale_
		});

		Matrix4x4 rotateMatrix;
		if (useBillboard_)
		{
			// ビルボード回転を使用
			rotateMatrix = billboardMatrix;
		}
		else
		{
			// クォータニオンから回転行列を作成
			Quaternion q(particle.rotation.x, particle.rotation.y, particle.rotation.z, particle.rotation.w);
			rotateMatrix = q.ToMatrix();
		}

		Matrix4x4 translateMatrix = MakeTranslateMatrix(particle.position);

		// ワールド行列合成
		Matrix4x4 worldMatrix = Multiply(scaleMatrix, Multiply(rotateMatrix, translateMatrix));

		// ビュープロジェクション行列取得
		const Matrix4x4& viewProj = activeCamera->GetViewProjectionMatrix();
		
		// WVP行列合成
		Matrix4x4 wvp = Multiply(worldMatrix, viewProj);

		// データを格納
		instanceData_[instanceCount_].world = worldMatrix;
		instanceData_[instanceCount_].worldViewProj = wvp;
		instanceData_[instanceCount_].color = particle.color;
		instanceData_[instanceCount_].uvOffsetScale = { 0.0f, 0.0f, 1.0f, 1.0f };

		instanceCount_++;
	}
}

void MeshRenderer::Draw(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	// プリミティブバッファの作成/再作成（needsRebuild_時）
	CreatePrimitiveBuffers(dxCommon);

	uint32_t drawCount = isGPUMode_ ? gpuParticleCount_ : instanceCount_;
	if (drawCount == 0) return;
	if (!primitiveVertexResource_ || primitiveMesh_.indices.empty()) return;

	// パイプラインステートの設定（ブレンドモード反映）
	auto* pm = ParticleManager::GetInstance();
	auto* plm = pm->GetPipelineManager();
	dxCommon->GetCommandList()->SetPipelineState(plm->GetPipelineState(blendMode_));
	dxCommon->GetCommandList()->SetGraphicsRootSignature(plm->GetRootSignature()); // RootSignature Binding

	dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &primitiveVertexView_);
	dxCommon->GetCommandList()->IASetIndexBuffer(&primitiveIndexView_);
	dxCommon->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// マテリアル (Slot 0 - CBV)
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

	// テクスチャ (Slot 2)
	dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, srvManager->GetGPUDescriptorHandle(textureIndex_));

	// インスタンシングデータ (Slot 1 - VertexShader用)
	uint32_t index = isGPUMode_ ? gpuSrvIndex_ : instanceSrvIndex_;
	dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(1, srvManager->GetGPUDescriptorHandle(index));

	dxCommon->GetCommandList()->DrawIndexedInstanced(
		static_cast<UINT>(primitiveMesh_.indices.size()),
		drawCount,
		0, 0, 0
	);
}

void MeshRenderer::Initialize(const std::string& texturePath)
{
	// 初期化時にデフォルトテクスチャを設定
	std::string path = texturePath.empty() ? "./Resources/uvChecker.png" : texturePath;
	TextureManager::GetInstance()->LoadTexture(path);
	textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(path);

	auto* pm = ParticleManager::GetInstance();
	InitializeBuffers(pm->GetDxCommon(), pm->GetSrvManager());
	CreatePrimitiveBuffers(pm->GetDxCommon());
}

void MeshRenderer::SetTexture(const std::string& texturePath)
{
	if (texturePath.empty()) return;

	TextureManager::GetInstance()->LoadTexture(texturePath);
	textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(texturePath);
}

void MeshRenderer::SetGPUMode(bool enable, uint32_t srvIndex, uint32_t count)
{
	isGPUMode_ = enable;
	gpuSrvIndex_ = srvIndex;
	gpuParticleCount_ = count;
}
