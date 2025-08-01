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

void Model::Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename, const std::string& modelType)
{
	modelCommon_ = modelCommon;

	std::string objFilePath = filename + "/" + filename + modelType;

	//モデルの読み込み
	modelData_ = LoadModelFile(directoryPath,objFilePath);

	//テクスチャの読み込み
	//.objの参照しているテクスチャファイル読み込み
	modelData_.material.textureFilePath = directoryPath + "/" + filename + "/" + modelData_.material.textureFilePath;
	TextureManager::GetInstance()->LoadTexture(modelData_.material.textureFilePath);
	//読み込んだテクスチャの番号を取得
	modelData_.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(modelData_.material.textureFilePath);

	//描画設定の初期化
	InitializeRenderingSettings();
}

void Model::Draw()
{
	//3D描画
	modelCommon_->GetDXCommon()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);
	//マテリアルCBufferの場所を設定
	modelCommon_->GetDXCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	//SRVのDescriptorTableの先頭を設定。2はrootPatameter[2]である。
	modelCommon_->GetDXCommon()->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(modelData_.material.textureIndex));
	//描画！
	modelCommon_->GetDXCommon()->GetCommandList()->DrawInstanced(UINT(modelData_.vertices.size()), 1, 0, 0);
}

MaterialData Model::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
{
	MaterialData materialData;
	std::string line;
	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		//identifierに応じた処理
		if (identifier == "map_Kd")
		{
			std::string textureFilename;
			s >> textureFilename;

			//連結してファイルパスにする
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}

	return materialData;
}

ModelData Model::LoadModelFile(const std::string& directoryPath, const std::string& filename)
{
	ModelData modelData;				//構築するModelData
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_FlipWindingOrder | aiProcess_FlipUVs);
	assert(scene->HasMeshes());

	//メッシュの解析
	for(uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
	{
		aiMesh* mesh = scene->mMeshes[meshIndex];
		assert(mesh->HasNormals());		//法線がないMeshは非対応
		assert(mesh->HasTextureCoords(0));	//テクスチャ座標がないMeshは非対応

		//faceの解析
		for(uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
		{
			aiFace& face = mesh->mFaces[faceIndex];
			assert(face.mNumIndices == 3);	//三角形以外は非対応

			//三角形の頂点を解析
			for(uint32_t element = 0; element < face.mNumIndices; ++element)
			{
				uint32_t vertexIndex = face.mIndices[element];
				aiVector3D& position = mesh->mVertices[vertexIndex];
				aiVector3D& normal = mesh->mNormals[vertexIndex];
				aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];
				VertexData vertex;
				vertex.position = { position.x,position.y,position.z,1.0f };
				vertex.normal = { normal.x,normal.y,normal.z };
				vertex.texcoord = { texcoord.x,texcoord.y };
				//airProcess_MakeLeftHandedはz*=-1で、右て->左手に変換するので手動で処理
				vertex.position.x *= -1.0f;
				vertex.normal.x *= -1.0f;
                modelData.vertices.push_back(vertex);
			}
		}
	}

	//Materialの解析
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

	//Nodeの解析
	modelData.rootNode = ReadNode(scene->mRootNode);

	return modelData;
}

void Model::CreateVertexData()
{
	/*--------------[ VertexResourceを作る ]-----------------*/

	vertexResource_ = modelCommon_->GetDXCommon()->CreateBufferResource(sizeof(VertexData) * modelData_.vertices.size());

	/*--------------[ VertexBufferViewを作る(値を設定するだけ) ]-----------------*/

	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();				//リソースの先頭のアドレスから使う
	vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());	//使用するリソースのサイズは頂点のサイズ
	vertexBufferView_.StrideInBytes = sizeof(VertexData);									//１頂点当たりのサイズ

	/*--------------[ VertexResourceにデータを書き込むためのアドレスを取得してvertexDataに割り当てる ]-----------------*/

	vertexResource_->Map(0,
		nullptr,
		reinterpret_cast<void**>(&vertexData_)
	);

	/*--------------[ モデルデータの頂点情報をコピーする ]-----------------*/

	std::memcpy(vertexData_, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
}

void Model::CreateMaterialData()
{
	/*--------------[ MaterialResourceを作る ]-----------------*/

	materialResource_ = modelCommon_->GetDXCommon()->CreateBufferResource(sizeof(Material));

	/*--------------[ MaterialResourceにデータを書き込むためのアドレスを取得してmaterialDataに割り当てる ]-----------------*/

	materialResource_->Map(0,
		nullptr,
		reinterpret_cast<void**>(&materialData_)
	);

	//マテリアルデータの初期値を書き込む
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->enableLighting = true;
	materialData_->uvTransform = MakeIdentity4x4();
	materialData_->shininess = 30.0f;
	materialData_->reflectivity = 0.0f;
}

void Model::InitializeRenderingSettings()
{
	//頂点データの生成
	CreateVertexData();

	//マテリアルデータの生成
	CreateMaterialData();
}

Node Model::ReadNode(aiNode* node)
{
	Node result;
	aiMatrix4x4 aiLocalMatrix = node->mTransformation;						//nodeのローカル行列を取得
	aiLocalMatrix.Transpose();												//列ベクトル形式を行ベクトル形式に変換
	//行列の要素をコピー
	for (int32_t row = 0; row < 4; ++row)
	{
		for (int32_t column = 0; column < 4; ++column)
		{
			result.localMatrix.m[row][column] = aiLocalMatrix[row][column];
		}
	}
	result.name = node->mName.C_Str();										//ノードの名前を取得
	result.children.resize(node->mNumChildren);								//子ノードの数だけ確保
	for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
	{
		//再帰的に読んで階層構造を作る
		result.children[childIndex] = ReadNode(node->mChildren[childIndex]);	//子ノードを再帰的に読み取る
	}
	return result;
}
