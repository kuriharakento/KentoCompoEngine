#include "Model.h"

#include <cassert>
#include <fstream>
#include <sstream>
#include <filesystem>

// math
#include "base/GraphicsTypes.h"
// Assimp
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
// manager
#include "manager/graphics/TextureManager.h"

// 行列の行数・列数
constexpr int kMatrixRows = 4;
constexpr int kMatrixColumns = 4;
// デフォルトの反射強度
constexpr float kDefaultShininess = 30.0f;
// デフォルトの反射率
constexpr float kDefaultReflectivity = 0.0f;
// 三角形の頂点数
constexpr int kTriangleVertices = 3;
// 座標の左手系変換係数
constexpr float kLeftHandConversion = -1.0f;
// 頂点座標のW成分
constexpr float kVertexW = 1.0f;
// デフォルトテクスチャパス
const std::string kDefaultTexturePath = "./Resources/white1x1.png";

Model::Model(const Model& other)
{
	// ModelCommonは同じものを使う（通常共有でOK）
	modelCommon_ = other.modelCommon_;

	// modelData_は単純コピーでOK（頂点・マテリアル情報など）
	modelData_ = other.modelData_;

	// メッシュリソースとマテリアルリソースを再生成
	CreateMeshResources();
	CreateMaterialResources();
}

void Model::Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename, const std::string& modelType)
{
	modelCommon_ = modelCommon;

	// モデルファイルのパスを構築
	std::string objFilePath = filename + "/" + filename + modelType;

	// ファイルが存在しない場合のフォールバック（engine/Resources/models）
	std::string resolvedDirectoryPath = directoryPath;
	if (!std::filesystem::exists(resolvedDirectoryPath + "/" + objFilePath))
	{
		std::string fallbackPath = "../engine/Resources/models";
		if (std::filesystem::exists(fallbackPath + "/" + objFilePath))
		{
			resolvedDirectoryPath = fallbackPath;
		}
	}

	// モデルの読み込み
	modelData_ = LoadModelFile(resolvedDirectoryPath, objFilePath);

	// モデルのベースパス
	std::string basePath = resolvedDirectoryPath + "/" + filename + "/";

	// 全マテリアルのテクスチャを読み込み
	for (auto& material : modelData_.materials)
	{
		if (!material.textureFilePath.empty())
		{
			// テクスチャのフルパスを構築
			std::string fullTexturePath = basePath + material.textureFilePath;
			material.textureFilePath = fullTexturePath;
			
			// テクスチャの読み込み
			TextureManager::GetInstance()->LoadTexture(fullTexturePath);
			material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(fullTexturePath);
		}
		else
		{
			// デフォルトテクスチャを使用
			TextureManager::GetInstance()->LoadTexture(kDefaultTexturePath);
			material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(kDefaultTexturePath);
		}
	}

	// マテリアルがない場合はデフォルトマテリアルを作成
	if (modelData_.materials.empty())
	{
		MaterialData defaultMaterial;
		defaultMaterial.name = "DefaultMaterial";
		TextureManager::GetInstance()->LoadTexture(kDefaultTexturePath);
		defaultMaterial.textureFilePath = kDefaultTexturePath;
		defaultMaterial.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(kDefaultTexturePath);
		modelData_.materials.push_back(defaultMaterial);
	}

	// 描画設定の初期化
	InitializeRenderingSettings();
}

void Model::Draw()
{
	auto* commandList = modelCommon_->GetDXCommon()->GetCommandList();

	// 全メッシュを描画
	for (auto& meshResource : meshResources_)
	{
		// 頂点バッファを設定
		commandList->IASetVertexBuffers(0, 1, &meshResource.vertexBufferView);

		// インデックスバッファを設定
		commandList->IASetIndexBuffer(&meshResource.indexBufferView);

		// マテリアルCBufferの場所を設定
		commandList->SetGraphicsRootConstantBufferView(0, meshResource.materialBuffer->GetGPUVirtualAddress());

		// テクスチャSRVをrootParameter[2]に設定
		commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(meshResource.textureIndex));

		// インデックス付き描画コマンドを発行
		commandList->DrawIndexedInstanced(meshResource.indexCount, 1, 0, 0, 0);
	}
}

void Model::DrawShadow()
{
	auto* commandList = modelCommon_->GetDXCommon()->GetCommandList();

	// 全メッシュを描画（マテリアル、テクスチャは不要）
	for (auto& meshResource : meshResources_)
	{
		// 頂点バッファを設定
		commandList->IASetVertexBuffers(0, 1, &meshResource.vertexBufferView);

		// インデックスバッファを設定
		commandList->IASetIndexBuffer(&meshResource.indexBufferView);

		// インデックス付き描画コマンドを発行
		commandList->DrawIndexedInstanced(meshResource.indexCount, 1, 0, 0, 0);
	}
}

void Model::DrawGBuffer()
{
	auto* commandList = modelCommon_->GetDXCommon()->GetCommandList();

	// 全メッシュを描画
	for (auto& meshResource : meshResources_)
	{
		// 頂点バッファを設定
		commandList->IASetVertexBuffers(0, 1, &meshResource.vertexBufferView);

		// インデックスバッファを設定
		commandList->IASetIndexBuffer(&meshResource.indexBufferView);

		// マテリアルCBufferの場所を設定（ルートパラメータ2: Material）
		commandList->SetGraphicsRootConstantBufferView(2, meshResource.materialBuffer->GetGPUVirtualAddress());

		// テクスチャSRVをrootParameter[3]に設定
		commandList->SetGraphicsRootDescriptorTable(3, TextureManager::GetInstance()->GetSrvHandleGPU(meshResource.textureIndex));

		// インデックス付き描画コマンドを発行
		commandList->DrawIndexedInstanced(meshResource.indexCount, 1, 0, 0, 0);
	}
}

MaterialData Model::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
{
	MaterialData materialData;
	std::string line;
	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	// ファイルを1行ずつ読み込み
	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		// テクスチャファイルパスを取得
		if (identifier == "map_Kd")
		{
			std::string textureFilename;
			s >> textureFilename;

			// フルパスを構築
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}

	return materialData;
}

ModelData Model::LoadModelFile(const std::string& directoryPath, const std::string& filename)
{
	ModelData modelData;
	Assimp::Importer importer;

	// モデルファイルを読み込み（ワインディングとUVを反転、三角形化）
	std::string filePath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), 
		aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_Triangulate);
	
	// シーンの検証
	assert(scene != nullptr && "Failed to load model file");
	assert(scene->HasMeshes() && "Model has no meshes");

	// マテリアルの解析（先に読み込む）
	for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex)
	{
		aiMaterial* aiMat = scene->mMaterials[materialIndex];
		MaterialData material;

		// マテリアル名を取得
		aiString matName;
		if (aiMat->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS)
		{
			material.name = matName.C_Str();
		}
		else
		{
			material.name = "Material_" + std::to_string(materialIndex);
		}

		// ディフューズテクスチャを取得
		if (aiMat->GetTextureCount(aiTextureType_DIFFUSE) > 0)
		{
			aiString textureFilePath;
			aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
			material.textureFilePath = textureFilePath.C_Str();
		}

		modelData.materials.push_back(material);
	}

	// メッシュの解析
	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
	{
		aiMesh* mesh = scene->mMeshes[meshIndex];
		MeshData meshData;

		// マテリアルインデックスを設定
		meshData.materialIndex = mesh->mMaterialIndex;

		// 頂点データの読み込み（インデックスなしで直接格納）
		meshData.vertices.reserve(mesh->mNumVertices);
		for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
		{
			VertexData vertex;
			
			// 位置
			aiVector3D& position = mesh->mVertices[vertexIndex];
			vertex.position = { position.x * kLeftHandConversion, position.y, position.z, kVertexW };

			// 法線
			if (mesh->HasNormals())
			{
				aiVector3D& normal = mesh->mNormals[vertexIndex];
				vertex.normal = { normal.x * kLeftHandConversion, normal.y, normal.z };
			}
			else
			{
				vertex.normal = { 0.0f, 1.0f, 0.0f };
			}

			// テクスチャ座標
			if (mesh->HasTextureCoords(0))
			{
				aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];
				vertex.texcoord = { texcoord.x, texcoord.y };
			}
			else
			{
				vertex.texcoord = { 0.0f, 0.0f };
			}

			meshData.vertices.push_back(vertex);
		}

		// インデックスデータの読み込み
		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
		{
			aiFace& face = mesh->mFaces[faceIndex];
			// 三角形のみ対応（aiProcess_Triangulateで保証）
			assert(face.mNumIndices == kTriangleVertices);

			for (uint32_t i = 0; i < face.mNumIndices; ++i)
			{
				meshData.indices.push_back(face.mIndices[i]);
			}
		}

		modelData.meshes.push_back(meshData);
	}

	// ノードの解析
	modelData.rootNode = ReadNode(scene->mRootNode);

	return modelData;
}

void Model::CreateMeshResources()
{
	meshResources_.resize(modelData_.meshes.size());

	for (size_t i = 0; i < modelData_.meshes.size(); ++i)
	{
		auto& mesh = modelData_.meshes[i];
		auto& resource = meshResources_[i];

		// 頂点バッファの作成
		size_t vertexSize = sizeof(VertexData) * mesh.vertices.size();
		resource.vertexBuffer = modelCommon_->GetDXCommon()->CreateBufferResource(vertexSize);

		// 頂点バッファビューの設定
		resource.vertexBufferView.BufferLocation = resource.vertexBuffer->GetGPUVirtualAddress();
		resource.vertexBufferView.SizeInBytes = static_cast<UINT>(vertexSize);
		resource.vertexBufferView.StrideInBytes = sizeof(VertexData);

		// 頂点データの書き込み
		VertexData* vertexData = nullptr;
		resource.vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
		std::memcpy(vertexData, mesh.vertices.data(), vertexSize);

		// インデックスバッファの作成
		size_t indexSize = sizeof(uint32_t) * mesh.indices.size();
		resource.indexBuffer = modelCommon_->GetDXCommon()->CreateBufferResource(indexSize);

		// インデックスバッファビューの設定
		resource.indexBufferView.BufferLocation = resource.indexBuffer->GetGPUVirtualAddress();
		resource.indexBufferView.SizeInBytes = static_cast<UINT>(indexSize);
		resource.indexBufferView.Format = DXGI_FORMAT_R32_UINT;

		// インデックスデータの書き込み
		uint32_t* indexData = nullptr;
		resource.indexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
		std::memcpy(indexData, mesh.indices.data(), indexSize);

		// インデックス数を保存
		resource.indexCount = static_cast<uint32_t>(mesh.indices.size());

		// マテリアルインデックスを保存
		resource.materialIndex = mesh.materialIndex;

		// テクスチャインデックスをマテリアルから取得
		if (mesh.materialIndex < modelData_.materials.size())
		{
			resource.textureIndex = modelData_.materials[mesh.materialIndex].textureIndex;
		}
	}
}

void Model::CreateMaterialResources()
{
	// 各メッシュにマテリアルバッファを作成
	for (size_t i = 0; i < meshResources_.size(); ++i)
	{
		auto& resource = meshResources_[i];

		// マテリアルバッファの作成
		resource.materialBuffer = modelCommon_->GetDXCommon()->CreateBufferResource(sizeof(Material));

		// マテリアルデータの書き込み
		resource.materialBuffer->Map(0, nullptr, reinterpret_cast<void**>(&resource.gpuMaterial));

		// マテリアルデータの初期値を設定
		resource.gpuMaterial->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		resource.gpuMaterial->enableLighting = true;
		resource.gpuMaterial->uvTransform = MakeIdentity4x4();
		resource.gpuMaterial->shininess = kDefaultShininess;
		resource.gpuMaterial->reflectivity = kDefaultReflectivity;
	}
}

void Model::InitializeRenderingSettings()
{
	// メッシュリソースの生成
	CreateMeshResources();

	// マテリアルリソースの生成
	CreateMaterialResources();
}

Node Model::ReadNode(aiNode* node)
{
	Node result;

	// ノードのローカル行列を取得し、転置して行ベクトル形式に変換
	aiMatrix4x4 aiLocalMatrix = node->mTransformation;
	aiLocalMatrix.Transpose();

	// 行列の要素をコピー
	for (int32_t row = 0; row < kMatrixRows; ++row)
	{
		for (int32_t column = 0; column < kMatrixColumns; ++column)
		{
			result.localMatrix.m[row][column] = aiLocalMatrix[row][column];
		}
	}

	// ノード名を取得
	result.name = node->mName.C_Str();

	// 子ノードの数だけ確保して再帰的に読み取り
	result.children.resize(node->mNumChildren);
	for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
	{
		result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
	}
	return result;
}

// アクセッサ実装

void Model::SetColor(const Vector4& color)
{
	for (auto& resource : meshResources_)
	{
		if (resource.gpuMaterial)
		{
			resource.gpuMaterial->color = color;
		}
	}
}

void Model::SetEnableLighting(bool enable)
{
	for (auto& resource : meshResources_)
	{
		if (resource.gpuMaterial)
		{
			resource.gpuMaterial->enableLighting = enable;
		}
	}
}

void Model::SetShininess(float shininess)
{
	for (auto& resource : meshResources_)
	{
		if (resource.gpuMaterial)
		{
			resource.gpuMaterial->shininess = shininess;
		}
	}
}

Vector3 Model::GetUVTranslate() const
{
	if (meshResources_.empty() || !meshResources_[0].gpuMaterial)
	{
		return Vector3(0, 0, 0);
	}
	return MathUtils::GetTranslateFromMatrix(meshResources_[0].gpuMaterial->uvTransform);
}

Vector3 Model::GetUVScale() const
{
	if (meshResources_.empty() || !meshResources_[0].gpuMaterial)
	{
		return Vector3(1, 1, 1);
	}
	return MathUtils::GetScaleFromMatrix(meshResources_[0].gpuMaterial->uvTransform);
}

Vector3 Model::GetUVRotate() const
{
	if (meshResources_.empty() || !meshResources_[0].gpuMaterial)
	{
		return Vector3(0, 0, 0);
	}
	return MathUtils::GetRotateFromMatrix(meshResources_[0].gpuMaterial->uvTransform);
}

void Model::SetUVTranslate(const Vector3& translate)
{
	for (auto& resource : meshResources_)
	{
		if (resource.gpuMaterial)
		{
			Vector3 scale = MathUtils::GetScaleFromMatrix(resource.gpuMaterial->uvTransform);
			Vector3 rotate = MathUtils::GetRotateFromMatrix(resource.gpuMaterial->uvTransform);
			resource.gpuMaterial->uvTransform = MakeAffineMatrix(scale, rotate, translate);
		}
	}
}

void Model::SetUVScale(const Vector3& scale)
{
	for (auto& resource : meshResources_)
	{
		if (resource.gpuMaterial)
		{
			Vector3 rotate = MathUtils::GetRotateFromMatrix(resource.gpuMaterial->uvTransform);
			Vector3 translate = MathUtils::GetTranslateFromMatrix(resource.gpuMaterial->uvTransform);
			resource.gpuMaterial->uvTransform = MakeAffineMatrix(scale, rotate, translate);
		}
	}
}

void Model::SetUVRotate(const Vector3& rotate)
{
	for (auto& resource : meshResources_)
	{
		if (resource.gpuMaterial)
		{
			Vector3 scale = MathUtils::GetScaleFromMatrix(resource.gpuMaterial->uvTransform);
			Vector3 translate = MathUtils::GetTranslateFromMatrix(resource.gpuMaterial->uvTransform);
			resource.gpuMaterial->uvTransform = MakeAffineMatrix(scale, rotate, translate);
		}
	}
}
