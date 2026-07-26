#include "SkinnedModel.h"
#include <cassert>
#include <Windows.h>
#include <cstring>

#include "manager/graphics/TextureManager.h"
#include "manager/graphics/SkinnedModelManager.h"
#include "base/DirectXCommon.h"

namespace KCE
{
// デフォルトテクスチャパス
const std::string kDefaultTexturePath = "./Resources/white1x1.png";
constexpr float kDefaultShininess = 30.0f;
constexpr float kDefaultReflectivity = 0.0f;

SkinnedModel::SkinnedModel(const SkinnedModel& other)
{
	modelCommon_ = other.modelCommon_;
	sharedResource_ = other.sharedResource_;

	CreateMeshResources();
	CreateMaterialResources();
	CreateSkinningBuffers();
}

void SkinnedModel::Initialize(ModelCommon* modelCommon, const std::string& directoryPath,
	const std::string& filename, const std::string& modelType)
{
	modelCommon_ = modelCommon;

	// マネージャーから共有リソースを取得
	sharedResource_ = SkinnedModelManager::GetInstance()->LoadModel(directoryPath, filename, modelType);
	assert(sharedResource_ && "Failed to load skinned model from manager");

	// 描画設定の初期化
	InitializeRenderingSettings();
}

void SkinnedModel::Draw()
{
	auto* commandList = modelCommon_->GetDXCommon()->GetCommandList();

	for (auto& meshResource : meshResources_)
	{
		// 変形済み頂点バッファを使用
		commandList->IASetVertexBuffers(0, 1, &meshResource.vertexBufferView);
		commandList->IASetIndexBuffer(&meshResource.indexBufferView);
		commandList->SetGraphicsRootConstantBufferView(0, meshResource.materialBuffer->GetGPUVirtualAddress());
		commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(meshResource.textureIndex));
		commandList->DrawIndexedInstanced(meshResource.indexCount, 1, 0, 0, 0);
	}
}

void SkinnedModel::DrawShadow()
{
	auto* commandList = modelCommon_->GetDXCommon()->GetCommandList();

	for (auto& meshResource : meshResources_)
	{
		commandList->IASetVertexBuffers(0, 1, &meshResource.vertexBufferView);
		commandList->IASetIndexBuffer(&meshResource.indexBufferView);
		commandList->DrawIndexedInstanced(meshResource.indexCount, 1, 0, 0, 0);
	}
}

void SkinnedModel::DrawGBuffer()
{
	auto* commandList = modelCommon_->GetDXCommon()->GetCommandList();

	for (auto& meshResource : meshResources_)
	{
		commandList->IASetVertexBuffers(0, 1, &meshResource.vertexBufferView);
		commandList->IASetIndexBuffer(&meshResource.indexBufferView);
		commandList->SetGraphicsRootConstantBufferView(2, meshResource.materialBuffer->GetGPUVirtualAddress());
		commandList->SetGraphicsRootDescriptorTable(3, TextureManager::GetInstance()->GetSrvHandleGPU(meshResource.textureIndex));
		commandList->DrawIndexedInstanced(meshResource.indexCount, 1, 0, 0, 0);
	}
}

void SkinnedModel::CreateMeshResources()
{
	meshResources_.resize(sharedResource_->meshes.size());

	for (size_t i = 0; i < sharedResource_->meshes.size(); ++i)
	{
		const auto& sharedMesh = sharedResource_->meshes[i];
		const auto& modelMeshData = sharedResource_->modelData.meshes[i];
		auto& resource = meshResources_[i];

		resource.vertexOffset = sharedMesh.vertexOffset;
		resource.indexCount = sharedMesh.indexCount;
		
		// インデックスバッファビューを共有リソースから設定
		resource.indexBufferView.BufferLocation = sharedMesh.indexBuffer->GetGPUVirtualAddress();
		resource.indexBufferView.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * sharedMesh.indexCount);
		resource.indexBufferView.Format = DXGI_FORMAT_R32_UINT;

		resource.materialIndex = modelMeshData.materialIndex;

		if (modelMeshData.materialIndex < sharedResource_->modelData.materials.size())
		{
			resource.textureIndex = sharedResource_->modelData.materials[modelMeshData.materialIndex].textureIndex;
		}
	}
}

void SkinnedModel::CreateMaterialResources()
{
	for (size_t i = 0; i < meshResources_.size(); ++i)
	{
		auto& resource = meshResources_[i];

		resource.materialBuffer = modelCommon_->GetDXCommon()->CreateBufferResource(sizeof(Material));
		resource.materialBuffer->Map(0, nullptr, reinterpret_cast<void**>(&resource.gpuMaterial));

		resource.gpuMaterial->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		resource.gpuMaterial->enableLighting = true;
		resource.gpuMaterial->uvTransform = MakeIdentity4x4();
		resource.gpuMaterial->shininess = kDefaultShininess;
		resource.gpuMaterial->reflectivity = kDefaultReflectivity;
	}
}

void SkinnedModel::CreateSkinningBuffers()
{
	// 出力バッファ（変形後VertexData）の作成
	// これはインスタンスごとに必要（アニメーション状態が異なるため）
	size_t outputSize = sizeof(VertexData) * sharedResource_->totalVertexCount;

	// DEFAULT heap for UAV support
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = outputSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	HRESULT hr = modelCommon_->GetDXCommon()->GetDevice()->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&skinnedVertexOutputBuffer_)
	);
	assert(SUCCEEDED(hr));

	// 頂点バッファビューを出力バッファに設定
	for (size_t i = 0; i < meshResources_.size(); ++i)
	{
		auto& resource = meshResources_[i];
		const auto& sharedMesh = sharedResource_->meshes[i]; // Use sharedMesh for vertexCount
		
		// 頂点バッファビュー（初期状態は出力バッファを指す）
		resource.vertexBufferView.BufferLocation = skinnedVertexOutputBuffer_->GetGPUVirtualAddress() +
			resource.vertexOffset * sizeof(VertexData);
		resource.vertexBufferView.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * sharedMesh.vertexCount);
		resource.vertexBufferView.StrideInBytes = sizeof(VertexData);
	}
}

void SkinnedModel::InitializeRenderingSettings()
{
	CreateMeshResources();
	CreateMaterialResources();
	CreateSkinningBuffers();
}

Vector4 SkinnedModel::GetColor() const
{
	if (meshResources_.empty() || !meshResources_[0].gpuMaterial)
	{
		return Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	}
	return meshResources_[0].gpuMaterial->color;
}

void SkinnedModel::SetColor(const Vector4& color)
{
	for (auto& resource : meshResources_)
	{
		if (resource.gpuMaterial)
		{
			resource.gpuMaterial->color = color;
		}
	}
}

bool SkinnedModel::IsEnableLighting() const
{
	if (meshResources_.empty() || !meshResources_[0].gpuMaterial)
	{
		return true;
	}
	return meshResources_[0].gpuMaterial->enableLighting;
}

void SkinnedModel::SetEnableLighting(bool enable)
{
	for (auto& resource : meshResources_)
	{
		if (resource.gpuMaterial)
		{
			resource.gpuMaterial->enableLighting = enable;
		}
	}
}
} // namespace KCE
