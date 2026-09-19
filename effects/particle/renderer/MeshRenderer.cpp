#include "effects/particle/renderer/MeshRenderer.h"
#include "effects/particle/gpu/GPUParticlePipeline.h"
#include "base/DirectXCommon.h"
#include "base/GraphicsTypes.h"
#include "base/Logger.h"
#include "manager/system/SrvManager.h"
#include "manager/scene/CameraManager.h"
#include "base/Camera.h"
#include "math/MatrixFunc.h"
#include "effects/particle/ParticleManager.h"
#include "manager/graphics/TextureManager.h"
#include "manager/effect/ParticlePipelineManager.h"
#include "effects/particle/diagnostics/ParticleDiagnostics.h"
#include <d3d12.h>
#include <DirectXTex/d3dx12.h>
#include <numbers>
#include <cstddef>

namespace KCE
{
MeshRenderer::~MeshRenderer()
{
	if (instanceResource_)
	{
		instanceResource_->Unmap(0, nullptr);
	}
	if (indexedDrawArgumentsUpload_ && indexedDrawArgumentsData_)
	{
		indexedDrawArgumentsUpload_->Unmap(0, nullptr);
		indexedDrawArgumentsData_ = nullptr;
	}
	if (instanceSrvIndex_ != SrvManager::kInvalidSrvIndex)
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
	// プリミティブメッシュを再生成
	primitiveMesh_ = PrimitiveGenerator::Generate(primitiveType_, options_);

	// 既存のバッファをリセット（次回Update時に再作成）
	primitiveVertexResource_.Reset();
	primitiveIndexResource_.Reset();

	needsRebuild_ = false;
}

void MeshRenderer::InitializeBuffers(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	// 既に初期化済みの場合はスキップ
	if (instanceResource_) return;

	// インスタンシングバッファの作成（CPUからの書き込み用）
	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeof(ParticleGPU) * kMaxInstances;
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
		IID_PPV_ARGS(&instanceResource_)
	);

	// インスタンスデータを永続的にマップ
	instanceResource_->Map(0, nullptr, reinterpret_cast<void**>(&instanceData_));

	// SRVディスクリプタを確保
	if (!srvManager->TryAllocate(instanceSrvIndex_))
	{
		Logger::Log("MeshRenderer initialization failed: descriptor heap exhausted\n", Logger::LogLevel::Error);
		return;
	}

	// 構造化バッファのSRVを作成
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

	// マテリアルバッファの初期化
	materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->uvTransform = MakeIdentity4x4();
	materialData_->enableLighting = false;

	D3D12_INDIRECT_ARGUMENT_DESC indirectArg{};
	indirectArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
	D3D12_COMMAND_SIGNATURE_DESC signatureDesc{};
	signatureDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
	signatureDesc.NumArgumentDescs = 1;
	signatureDesc.pArgumentDescs = &indirectArg;
	dxCommon->GetDevice()->CreateCommandSignature(&signatureDesc, nullptr, IID_PPV_ARGS(&indexedDrawCommandSignature_));

	D3D12_RESOURCE_DESC argsDesc{};
	argsDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	argsDesc.Width = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
	argsDesc.Height = 1;
	argsDesc.DepthOrArraySize = 1;
	argsDesc.MipLevels = 1;
	argsDesc.SampleDesc.Count = 1;
	argsDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	D3D12_HEAP_PROPERTIES defaultHeap{};
	defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
	dxCommon->GetDevice()->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &argsDesc,
		D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&indexedDrawArguments_));
	D3D12_HEAP_PROPERTIES uploadHeap{};
	uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
	dxCommon->GetDevice()->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &argsDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&indexedDrawArgumentsUpload_));
	indexedDrawArgumentsUpload_->Map(0, nullptr, reinterpret_cast<void**>(&indexedDrawArgumentsData_));
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
	ParticleScopeTimer timer(ParticleProfileScope::RendererUpdatePerCall);

	if (isGPUMode_)
	{
		instanceCount_ = 0;
		return;
	}

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

		Matrix4x4 localRotMatrix = MakeRotateMatrix(particle.rotation);
		Matrix4x4 rotateMatrix;
		if (useBillboard_)
		{
			// パーティクルのローカル回転を適用してからビルボード回転
			rotateMatrix = Multiply(localRotMatrix, billboardMatrix);
		}
		else
		{
			rotateMatrix = localRotMatrix;
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
	const uint32_t particleSrvIndex = isGPUMode_ ? gpuSrvIndex_ : instanceSrvIndex_;
	if (!srvManager || !srvManager->IsAllocated(particleSrvIndex) || !srvManager->IsAllocated(textureIndex_)) return;
	ParticleScopeTimer timer(ParticleProfileScope::RendererDrawRecordingPerCall);

	// プリミティブバッファの作成/再作成（needsRebuild_時）
	CreatePrimitiveBuffers(dxCommon);

	uint32_t drawCount = isGPUMode_ ? gpuParticleCount_ : instanceCount_;
	if (drawCount == 0) return;
	if (!primitiveVertexResource_ || primitiveMesh_.indices.empty()) return;

	// パイプラインステートの設定（ブレンドモード反映）
	auto* pm = ParticleManager::GetInstance();
	auto* plm = pm->GetPipelineManager();
	dxCommon->GetCommandList()->SetPipelineState(plm->GetPipelineState(blendMode_, emissiveSettings_.enabled));
	dxCommon->GetCommandList()->SetGraphicsRootSignature(plm->GetRootSignature()); // RootSignature Binding

	dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &primitiveVertexView_);
	dxCommon->GetCommandList()->IASetIndexBuffer(&primitiveIndexView_);
	dxCommon->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ティントカラーをマテリアルに適用
	if (materialData_)
	{
		materialData_->color = tintColor_;
		materialData_->emissiveColorIntensity = { emissiveSettings_.color.x, emissiveSettings_.color.y, emissiveSettings_.color.z, emissiveSettings_.intensity };
		materialData_->emissiveEnabled = emissiveSettings_.enabled ? 1u : 0u;
		materialData_->emissiveSource = static_cast<uint32_t>(emissiveSettings_.source);
		materialData_->bloomContribution = emissiveSettings_.bloomContribution;
	}

	// マテリアル (Slot 0 - CBV)
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

	// テクスチャ (Slot 2)
	dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, srvManager->GetGPUDescriptorHandle(textureIndex_));
	dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(4, srvManager->GetGPUDescriptorHandle(emissiveTextureIndex_));

	// インスタンシングデータ (Slot 1 - VertexShader用)
	dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(1, srvManager->GetGPUDescriptorHandle(particleSrvIndex));

	auto* commandList = dxCommon->GetCommandList();
	if (isGPUMode_ && gpuDrawArguments_ && indexedDrawCommandSignature_ && indexedDrawArgumentsData_)
	{
		*indexedDrawArgumentsData_ = { static_cast<UINT>(primitiveMesh_.indices.size()), 0, 0, 0, 0 };
		if (indexedDrawArgumentsState_ != D3D12_RESOURCE_STATE_COPY_DEST)
		{
			D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				indexedDrawArguments_.Get(), indexedDrawArgumentsState_, D3D12_RESOURCE_STATE_COPY_DEST);
			commandList->ResourceBarrier(1, &barrier);
		}
		commandList->CopyBufferRegion(indexedDrawArguments_.Get(), 0, indexedDrawArgumentsUpload_.Get(), 0,
			sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
		D3D12_RESOURCE_BARRIER commonBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
			gpuDrawArguments_, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList->ResourceBarrier(1, &commonBarrier);
		commandList->CopyBufferRegion(indexedDrawArguments_.Get(), offsetof(D3D12_DRAW_INDEXED_ARGUMENTS, InstanceCount),
			gpuDrawArguments_, offsetof(D3D12_DRAW_ARGUMENTS, InstanceCount), sizeof(UINT));
		D3D12_RESOURCE_BARRIER barriers[2] = {
			CD3DX12_RESOURCE_BARRIER::Transition(gpuDrawArguments_, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT),
			CD3DX12_RESOURCE_BARRIER::Transition(indexedDrawArguments_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)
		};
		commandList->ResourceBarrier(2, barriers);
		indexedDrawArgumentsState_ = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
		commandList->ExecuteIndirect(indexedDrawCommandSignature_.Get(), 1, indexedDrawArguments_.Get(), 0, nullptr, 0);
	}
	else
	{
		commandList->DrawIndexedInstanced(static_cast<UINT>(primitiveMesh_.indices.size()), drawCount, 0, 0, 0);
	}
}

void MeshRenderer::Initialize(const std::string& texturePath)
{
	// 初期化時にデフォルトテクスチャを設定
	std::string path = texturePath.empty() ? "./Resources/uvChecker.png" : texturePath;
	texturePath_ = path;
	TextureManager::GetInstance()->LoadTexture(path);
	TextureManager::GetInstance()->TryGetTextureIndexByFilePath(path, textureIndex_);
	SetEmissiveTexture("");

	auto* pm = ParticleManager::GetInstance();
	InitializeBuffers(pm->GetDxCommon(), pm->GetSrvManager());
	CreatePrimitiveBuffers(pm->GetDxCommon());
}

void MeshRenderer::SetTexture(const std::string& texturePath)
{
	if (texturePath.empty()) return;

	auto* textures = TextureManager::GetInstance();
	textures->LoadTexture(texturePath);
	uint32_t newIndex = SrvManager::kInvalidSrvIndex;
	if (!textures->TryGetTextureIndexByFilePath(texturePath, newIndex)) return;
	texturePath_ = texturePath;
	textureIndex_ = newIndex;
}

void MeshRenderer::SetEmissiveTexture(const std::string& texturePath)
{
	emissiveTexturePath_ = texturePath;
	const std::string fallback = "./Resources/textures/emissive_black_1x1.png";
	const std::string path = emissiveTexturePath_.empty() || !TextureManager::GetInstance()->CheckTextureExists(emissiveTexturePath_) ? fallback : emissiveTexturePath_;
	TextureManager::GetInstance()->LoadTextureLinear(path);
	emissiveTextureIndex_ = TextureManager::GetInstance()->GetLinearTextureIndexByFilePath(path);
}

void MeshRenderer::SetGPUMode(bool enable, uint32_t srvIndex, uint32_t count, ID3D12Resource* drawArguments)
{
	isGPUMode_ = enable;
	gpuSrvIndex_ = srvIndex;
	gpuParticleCount_ = count;
	gpuDrawArguments_ = drawArguments;
}
} // namespace KCE
