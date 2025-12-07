#include "Model.h"

#include <cassert>
#include <fstream>
#include <sstream>

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

Model::Model(const Model& other)
{
	// ModelCommonは同じものを使う（通常共有でOK）
	modelCommon_ = other.modelCommon_;

	// modelData_は単純コピーでOK（頂点・マテリアル情報など）
	modelData_ = other.modelData_;

	// 頂点データをもう一度Create
	CreateVertexData();

	// マテリアルデータももう一度Create
	CreateMaterialData();
}

void Model::Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename, const std::string& modelType)
{
	modelCommon_ = modelCommon;

	// モデルファイルのパスを構築
	std::string objFilePath = filename + "/" + filename + modelType;

	// モデルの読み込み
	modelData_ = LoadModelFile(directoryPath,objFilePath);

	// テクスチャのファイルパスを構築して読み込み
	modelData_.material.textureFilePath = directoryPath + "/" + filename + "/" + modelData_.material.textureFilePath;
	TextureManager::GetInstance()->LoadTexture(modelData_.material.textureFilePath);

	// 読み込んだテクスチャの番号を取得
	modelData_.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(modelData_.material.textureFilePath);

	// 描画設定の初期化
	InitializeRenderingSettings();
}

void Model::Draw()
{
	// 頂点バッファを設定
	modelCommon_->GetDXCommon()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);

	// マテリアルCBufferの場所を設定
	modelCommon_->GetDXCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

	// テクスチャSRVをrootParameter[2]に設定
	modelCommon_->GetDXCommon()->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(modelData_.material.textureIndex));

	// 描画コマンドを発行
	modelCommon_->GetDXCommon()->GetCommandList()->DrawInstanced(UINT(modelData_.vertices.size()), 1, 0, 0);
}

void Model::DrawShadow()
{
	// 頂点バッファのみを設定（マテリアル、テクスチャは不要）
	modelCommon_->GetDXCommon()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);

	// 描画コマンドを発行
	modelCommon_->GetDXCommon()->GetCommandList()->DrawInstanced(UINT(modelData_.vertices.size()), 1, 0, 0);
}

void Model::DrawGBuffer()
{
	auto* commandList = modelCommon_->GetDXCommon()->GetCommandList();
	
	// 頂点バッファを設定
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

	// マテリアルCBufferの場所を設定（ルートパラメータ2: Material）
	commandList->SetGraphicsRootConstantBufferView(2, materialResource_->GetGPUVirtualAddress());

	// テクスチャSRVをrootParameter[3]に設定
	commandList->SetGraphicsRootDescriptorTable(3, TextureManager::GetInstance()->GetSrvHandleGPU(modelData_.material.textureIndex));

	// 描画コマンドを発行
	commandList->DrawInstanced(UINT(modelData_.vertices.size()), 1, 0, 0);
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

	// モデルファイルを読み込み（ワインディングとUVを反転）
	std::string filePath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_FlipWindingOrder | aiProcess_FlipUVs);
	assert(scene->HasMeshes());

	// メッシュの解析
	for(uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
	{
		aiMesh* mesh = scene->mMeshes[meshIndex];
		assert(mesh->HasNormals());
		assert(mesh->HasTextureCoords(0));

		// フェイスの解析
		for(uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
		{
			aiFace& face = mesh->mFaces[faceIndex];
			// 三角形のみ対応
			assert(face.mNumIndices == kTriangleVertices);

			// 三角形の頂点を解析
			for(uint32_t element = 0; element < face.mNumIndices; ++element)
			{
				uint32_t vertexIndex = face.mIndices[element];
				aiVector3D& position = mesh->mVertices[vertexIndex];
				aiVector3D& normal = mesh->mNormals[vertexIndex];
				aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];
				VertexData vertex;
				vertex.position = { position.x, position.y, position.z, kVertexW };
				vertex.normal = { normal.x, normal.y, normal.z };
				vertex.texcoord = { texcoord.x, texcoord.y };

				// 右手座標系から左手座標系に変換（X軸を反転）
				vertex.position.x *= kLeftHandConversion;
				vertex.normal.x *= kLeftHandConversion;
                modelData.vertices.push_back(vertex);
			}
		}
	}

	// マテリアルの解析
	for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex)
	{
		aiMaterial* material = scene->mMaterials[materialIndex];
		if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0)
		{
			aiString textureFilePath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
			modelData.material.textureFilePath = textureFilePath.C_Str();
		}
	}

	// ノードの解析
	modelData.rootNode = ReadNode(scene->mRootNode);

	return modelData;
}

void Model::CreateVertexData()
{
	// VertexResourceを作成
	vertexResource_ = modelCommon_->GetDXCommon()->CreateBufferResource(sizeof(VertexData) * modelData_.vertices.size());

	// VertexBufferViewを設定
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	// VertexResourceにデータを書き込むためのアドレスを取得
	vertexResource_->Map(0,
		nullptr,
		reinterpret_cast<void**>(&vertexData_)
	);

	// モデルデータの頂点情報をコピー
	std::memcpy(vertexData_, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
}

void Model::CreateMaterialData()
{
	// MaterialResourceを作成
	materialResource_ = modelCommon_->GetDXCommon()->CreateBufferResource(sizeof(Material));

	// MaterialResourceにデータを書き込むためのアドレスを取得
	materialResource_->Map(0,
		nullptr,
		reinterpret_cast<void**>(&materialData_)
	);

	// マテリアルデータの初期値を設定
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->enableLighting = true;
	materialData_->uvTransform = MakeIdentity4x4();
	materialData_->shininess = kDefaultShininess;
	materialData_->reflectivity = kDefaultReflectivity;
}

void Model::InitializeRenderingSettings()
{
	// 頂点データの生成
	CreateVertexData();

	// マテリアルデータの生成
	CreateMaterialData();
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
