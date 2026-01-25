#include "SkinnedModel.h"

#include <cassert>
#include <cmath>
#include <Windows.h>

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

SkinnedModel::SkinnedModel(const SkinnedModel& other)
{
	modelCommon_ = other.modelCommon_;
	modelData_ = other.modelData_;
	totalVertexCount_ = other.totalVertexCount_;

	CreateMeshResources();
	CreateMaterialResources();
	CreateSkinningBuffers();
}

void SkinnedModel::Initialize(ModelCommon* modelCommon, const std::string& directoryPath,
	const std::string& filename, const std::string& modelType)
{
	modelCommon_ = modelCommon;

	// モデルファイルのパスを構築
	std::string filePath = filename + "/" + filename + modelType;

	// モデルの読み込み
	modelData_ = LoadSkinnedModelFile(directoryPath, filePath);

	// モデルのベースパス
	std::string basePath = directoryPath + "/" + filename + "/";

	// 全マテリアルのテクスチャを読み込み
	for (auto& material : modelData_.materials)
	{
		if (!material.textureFilePath.empty())
		{
			std::string fullTexturePath = basePath + material.textureFilePath;
			material.textureFilePath = fullTexturePath;
			TextureManager::GetInstance()->LoadTexture(fullTexturePath);
			material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(fullTexturePath);
		}
		else
		{
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

SkinnedModelData SkinnedModel::LoadSkinnedModelFile(const std::string& directoryPath, const std::string& filename)
{
	SkinnedModelData modelData;
	Assimp::Importer importer;

	std::string filePath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filePath.c_str(),
		aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_Triangulate |
		aiProcess_LimitBoneWeights | aiProcess_JoinIdenticalVertices);

	assert(scene != nullptr && "Failed to load skinned model file");
	assert(scene->HasMeshes() && "Skinned model has no meshes");

	// ボーン情報を抽出
	ExtractBones(scene, modelData);

	// マテリアルの解析
	for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex)
	{
		aiMaterial* aiMat = scene->mMaterials[materialIndex];
		MaterialData material;

		aiString matName;
		if (aiMat->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS)
		{
			material.name = matName.C_Str();
		}
		else
		{
			material.name = "Material_" + std::to_string(materialIndex);
		}

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
		SkinnedMeshData meshData;
		meshData.materialIndex = mesh->mMaterialIndex;

		// 頂点データの読み込み
		meshData.vertices.resize(mesh->mNumVertices);
		for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
		{
			SkinnedVertexData& vertex = meshData.vertices[i];

			// 位置
			aiVector3D& position = mesh->mVertices[i];
			vertex.position = { position.x * kLeftHandConversion, position.y, position.z, kVertexW };

			// 法線
			if (mesh->HasNormals())
			{
				aiVector3D& normal = mesh->mNormals[i];
				vertex.normal = { normal.x * kLeftHandConversion, normal.y, normal.z };
			}

			// テクスチャ座標
			if (mesh->HasTextureCoords(0))
			{
				aiVector3D& texcoord = mesh->mTextureCoords[0][i];
				vertex.texcoord = { texcoord.x, texcoord.y };
			}

			// ボーンウェイトは後で設定
		}

		// インデックスデータの読み込み
		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
		{
			aiFace& face = mesh->mFaces[faceIndex];
			assert(face.mNumIndices == kTriangleVertices);
			for (uint32_t i = 0; i < face.mNumIndices; ++i)
			{
				meshData.indices.push_back(face.mIndices[i]);
			}
		}

		// ボーンウェイトの抽出
		ExtractBoneWeights(mesh, modelData.skeleton, meshData);

		modelData.meshes.push_back(meshData);
	}

	// アニメーションの抽出
	ExtractAnimations(scene, modelData);

	// ノードの解析
	modelData.rootNode = ReadNode(scene->mRootNode);

	return modelData;
}

void SkinnedModel::ExtractBones(const aiScene* scene, SkinnedModelData& modelData)
{
	// 全メッシュからボーン情報を収集
	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
	{
		aiMesh* mesh = scene->mMeshes[meshIndex];

		for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
		{
			aiBone* bone = mesh->mBones[boneIndex];
			std::string boneName = bone->mName.C_Str();

			// 既に追加済みかチェック
			if (modelData.skeleton.boneNameToIndex.find(boneName) == modelData.skeleton.boneNameToIndex.end())
			{
				BoneInfo boneInfo;
				boneInfo.name = boneName;
				boneInfo.parentIndex = -1; // 後で設定

				// オフセット行列を取得し、転置
				aiMatrix4x4 aiOffsetMatrix = bone->mOffsetMatrix;
				aiOffsetMatrix.Transpose();
				for (int32_t row = 0; row < kMatrixRows; ++row)
				{
					for (int32_t col = 0; col < kMatrixColumns; ++col)
					{
						boneInfo.offsetMatrix.m[row][col] = aiOffsetMatrix[row][col];
					}
				}

				// 左手系座標変換 (X軸反転)
				// M' = S * M * S (S = Scale(-1, 1, 1))
				boneInfo.offsetMatrix.m[0][1] *= -1.0f;
				boneInfo.offsetMatrix.m[0][2] *= -1.0f;
				boneInfo.offsetMatrix.m[1][0] *= -1.0f;
				boneInfo.offsetMatrix.m[2][0] *= -1.0f;
				boneInfo.offsetMatrix.m[3][0] *= -1.0f;

				uint32_t newIndex = static_cast<uint32_t>(modelData.skeleton.bones.size());
				modelData.skeleton.boneNameToIndex[boneName] = newIndex;
				modelData.skeleton.bones.push_back(boneInfo);
			}
		}
	}

	// Armatureノード（スケルトンルート）を探してトランスフォームを取得
	aiNode* armatureNode = nullptr;
	if (scene->mRootNode)
	{
		// ルートノードのスケールを確認
		aiVector3D rootScaling, rootPosition;
		aiQuaternion rootRotation;
		scene->mRootNode->mTransformation.Decompose(rootScaling, rootRotation, rootPosition);
		
		char debugMsg[512];
		sprintf_s(debugMsg, "[SkinnedModel] Root node: '%s', Scale: (%.4f, %.4f, %.4f)\n", 
			scene->mRootNode->mName.C_Str(), rootScaling.x, rootScaling.y, rootScaling.z);
		OutputDebugStringA(debugMsg);
		
		// ルートノード自体が非1スケールを持つ場合、それがArmature
		if (std::abs(rootScaling.x - 1.0f) > 0.001f || 
			std::abs(rootScaling.y - 1.0f) > 0.001f || 
			std::abs(rootScaling.z - 1.0f) > 0.001f)
		{
			armatureNode = scene->mRootNode;
			OutputDebugStringA("[SkinnedModel] Using ROOT NODE as Armature (has non-1 scale)\n");
		}
		else
		{
			// 子ノードから非1スケールを持つノードを探す
			for (uint32_t i = 0; i < scene->mRootNode->mNumChildren; ++i)
			{
				aiNode* child = scene->mRootNode->mChildren[i];
				aiVector3D scaling, position;
				aiQuaternion rotation;
				child->mTransformation.Decompose(scaling, rotation, position);
				
				if (std::abs(scaling.x - 1.0f) > 0.001f || 
					std::abs(scaling.y - 1.0f) > 0.001f || 
					std::abs(scaling.z - 1.0f) > 0.001f)
				{
					armatureNode = child;
					break;
				}
			}
			
			// 非1スケールのノードが見つからない場合は、ボーンを持つノードを探す
			if (!armatureNode)
			{
				for (uint32_t i = 0; i < scene->mRootNode->mNumChildren; ++i)
				{
					aiNode* child = scene->mRootNode->mChildren[i];
					std::string childName = child->mName.C_Str();
					
					bool hasBoneChild = false;
					for (uint32_t j = 0; j < child->mNumChildren; ++j)
					{
						std::string grandchildName = child->mChildren[j]->mName.C_Str();
						if (modelData.skeleton.GetBoneIndex(grandchildName) >= 0)
						{
							hasBoneChild = true;
							break;
						}
					}
					
					if (hasBoneChild || childName.find("Armature") != std::string::npos)
					{
						armatureNode = child;
						break;
					}
				}
			}
		}
	}

	// Armatureノードのトランスフォームをスケルトンに保存
	if (armatureNode)
	{
		// デバッグ: Armatureノード名とスケールを出力
		aiVector3D scaling, position;
		aiQuaternion rotation;
		armatureNode->mTransformation.Decompose(scaling, rotation, position);
		
		char debugMsg[256];
		sprintf_s(debugMsg, "[SkinnedModel] Armature found: '%s', Scale: (%.4f, %.4f, %.4f)\n", 
			armatureNode->mName.C_Str(), scaling.x, scaling.y, scaling.z);
		OutputDebugStringA(debugMsg);

		aiMatrix4x4 aiArmatureMatrix = armatureNode->mTransformation;
		aiArmatureMatrix.Transpose();
		for (int32_t row = 0; row < kMatrixRows; ++row)
		{
			for (int32_t col = 0; col < kMatrixColumns; ++col)
			{
				modelData.skeleton.armatureTransform.m[row][col] = aiArmatureMatrix[row][col];
			}
		}
		// 左手系座標変換 (X軸反転)
		modelData.skeleton.armatureTransform.m[0][1] *= -1.0f;
		modelData.skeleton.armatureTransform.m[0][2] *= -1.0f;
		modelData.skeleton.armatureTransform.m[1][0] *= -1.0f;
		modelData.skeleton.armatureTransform.m[2][0] *= -1.0f;
		modelData.skeleton.armatureTransform.m[3][0] *= -1.0f;
	}
	else
	{
		OutputDebugStringA("[SkinnedModel] WARNING: Armature node not found!\n");
	}

	// 親ボーンインデックスとデフォルトローカル変換を設定（ノードツリーから）
	std::function<void(aiNode*, int32_t)> findParents = [&](aiNode* node, int32_t parentBoneIndex)
	{
		std::string nodeName = node->mName.C_Str();
		int32_t currentBoneIndex = modelData.skeleton.GetBoneIndex(nodeName);

		if (currentBoneIndex >= 0)
		{
			modelData.skeleton.bones[currentBoneIndex].parentIndex = parentBoneIndex;
			
			// ノードのローカル変換をデフォルトローカル変換として保存
			aiMatrix4x4 aiLocalMatrix = node->mTransformation;
			aiLocalMatrix.Transpose();
			Matrix4x4 localTransform;
			for (int32_t row = 0; row < kMatrixRows; ++row)
			{
				for (int32_t col = 0; col < kMatrixColumns; ++col)
				{
					localTransform.m[row][col] = aiLocalMatrix[row][col];
				}
			}
			
			// 左手系座標変換 (X軸反転)
			localTransform.m[0][1] *= -1.0f;
			localTransform.m[0][2] *= -1.0f;
			localTransform.m[1][0] *= -1.0f;
			localTransform.m[2][0] *= -1.0f;
			localTransform.m[3][0] *= -1.0f;
			
			modelData.skeleton.bones[currentBoneIndex].defaultLocalTransform = localTransform;
			
			parentBoneIndex = currentBoneIndex;
		}

		for (uint32_t i = 0; i < node->mNumChildren; ++i)
		{
			findParents(node->mChildren[i], parentBoneIndex);
		}
	};

	if (scene->mRootNode)
	{
		findParents(scene->mRootNode, -1);
	}
}

void SkinnedModel::ExtractAnimations(const aiScene* scene, SkinnedModelData& modelData)
{
	for (uint32_t animIndex = 0; animIndex < scene->mNumAnimations; ++animIndex)
	{
		aiAnimation* aiAnim = scene->mAnimations[animIndex];
		AnimationClip clip;
		clip.name = aiAnim->mName.C_Str();
		clip.ticksPerSecond = static_cast<float>(aiAnim->mTicksPerSecond > 0 ? aiAnim->mTicksPerSecond : 25.0);
		clip.duration = static_cast<float>(aiAnim->mDuration / clip.ticksPerSecond);

		for (uint32_t channelIndex = 0; channelIndex < aiAnim->mNumChannels; ++channelIndex)
		{
			aiNodeAnim* nodeAnim = aiAnim->mChannels[channelIndex];
			AnimationChannel channel;
			channel.boneName = nodeAnim->mNodeName.C_Str();
			channel.boneIndex = modelData.skeleton.GetBoneIndex(channel.boneName);

			// 位置キーフレーム（左手系座標変換のみ適用、スケールはarmatureTransformで処理）
			for (uint32_t i = 0; i < nodeAnim->mNumPositionKeys; ++i)
			{
				aiVectorKey& key = nodeAnim->mPositionKeys[i];
				AnimationKey<Vector3> posKey;
				posKey.time = static_cast<float>(key.mTime / clip.ticksPerSecond);
				// 左手系座標変換のみ適用（スケールはAnimatorでarmatureTransformを適用）
				posKey.value = { 
					key.mValue.x * kLeftHandConversion, 
					key.mValue.y, 
					key.mValue.z 
				};
				channel.positionKeys.push_back(posKey);
			}

			// 回転キーフレーム
			for (uint32_t i = 0; i < nodeAnim->mNumRotationKeys; ++i)
			{
				aiQuatKey& key = nodeAnim->mRotationKeys[i];
				AnimationKey<Quaternion> rotKey;
				rotKey.time = static_cast<float>(key.mTime / clip.ticksPerSecond);
				// 左手系変換：X軸ミラーに対応するクォータニオン変換
				// M' = S * M * S (S = diag(-1,1,1,1)) と一致させるため (x, -y, -z, w) を使用
				rotKey.value = Quaternion(key.mValue.x, -key.mValue.y, -key.mValue.z, key.mValue.w);
				channel.rotationKeys.push_back(rotKey);
			}

			// スケールキーフレーム
			for (uint32_t i = 0; i < nodeAnim->mNumScalingKeys; ++i)
			{
				aiVectorKey& key = nodeAnim->mScalingKeys[i];
				AnimationKey<Vector3> scaleKey;
				scaleKey.time = static_cast<float>(key.mTime / clip.ticksPerSecond);
				scaleKey.value = { key.mValue.x, key.mValue.y, key.mValue.z };
				channel.scaleKeys.push_back(scaleKey);
			}

			clip.channels.push_back(channel);
		}

		modelData.animations.push_back(clip);
	}
}

void SkinnedModel::ExtractBoneWeights(aiMesh* mesh, const Skeleton& skeleton, SkinnedMeshData& meshData)
{
	for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
	{
		aiBone* bone = mesh->mBones[boneIndex];
		std::string boneName = bone->mName.C_Str();
		int32_t skeletonBoneIndex = skeleton.GetBoneIndex(boneName);

		if (skeletonBoneIndex < 0) continue;

		for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex)
		{
			aiVertexWeight& vertexWeight = bone->mWeights[weightIndex];
			uint32_t vertexId = vertexWeight.mVertexId;
			float weight = vertexWeight.mWeight;

			// 空いているウェイトスロットを探して設定
			SkinnedVertexData& vertex = meshData.vertices[vertexId];
			for (uint32_t i = 0; i < kMaxBoneInfluence; ++i)
			{
				if (vertex.boneWeights[i] == 0.0f)
				{
					vertex.boneIndices[i] = static_cast<uint32_t>(skeletonBoneIndex);
					vertex.boneWeights[i] = weight;
					break;
				}
			}
		}
	}

	// ウェイトの正規化
	for (auto& vertex : meshData.vertices)
	{
		float totalWeight = 0.0f;
		for (uint32_t i = 0; i < kMaxBoneInfluence; ++i)
		{
			totalWeight += vertex.boneWeights[i];
		}
		if (totalWeight > 0.0f)
		{
			for (uint32_t i = 0; i < kMaxBoneInfluence; ++i)
			{
				vertex.boneWeights[i] /= totalWeight;
			}
		}
		else
		{
			// ウェイトがない場合はルートボーンに100%
			vertex.boneIndices[0] = 0;
			vertex.boneWeights[0] = 1.0f;
		}
	}
}

Node SkinnedModel::ReadNode(aiNode* node)
{
	Node result;

	aiMatrix4x4 aiLocalMatrix = node->mTransformation;
	aiLocalMatrix.Transpose();

	for (int32_t row = 0; row < kMatrixRows; ++row)
	{
		for (int32_t col = 0; col < kMatrixColumns; ++col)
		{
			result.localMatrix.m[row][col] = aiLocalMatrix[row][col];
		}
	}

	result.name = node->mName.C_Str();
	result.children.resize(node->mNumChildren);
	for (uint32_t i = 0; i < node->mNumChildren; ++i)
	{
		result.children[i] = ReadNode(node->mChildren[i]);
	}

	return result;
}

void SkinnedModel::CreateMeshResources()
{
	meshResources_.resize(modelData_.meshes.size());
	totalVertexCount_ = 0;

	// まず全頂点数を計算
	for (const auto& mesh : modelData_.meshes)
	{
		totalVertexCount_ += static_cast<uint32_t>(mesh.vertices.size());
	}

	uint32_t currentOffset = 0;
	for (size_t i = 0; i < modelData_.meshes.size(); ++i)
	{
		auto& mesh = modelData_.meshes[i];
		auto& resource = meshResources_[i];

		resource.vertexOffset = currentOffset;
		currentOffset += static_cast<uint32_t>(mesh.vertices.size());

		// インデックスバッファの作成
		size_t indexSize = sizeof(uint32_t) * mesh.indices.size();
		resource.indexBuffer = modelCommon_->GetDXCommon()->CreateBufferResource(indexSize);

		resource.indexBufferView.BufferLocation = resource.indexBuffer->GetGPUVirtualAddress();
		resource.indexBufferView.SizeInBytes = static_cast<UINT>(indexSize);
		resource.indexBufferView.Format = DXGI_FORMAT_R32_UINT;

		uint32_t* indexData = nullptr;
		resource.indexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
		std::memcpy(indexData, mesh.indices.data(), indexSize);

		resource.indexCount = static_cast<uint32_t>(mesh.indices.size());
		resource.materialIndex = mesh.materialIndex;

		if (mesh.materialIndex < modelData_.materials.size())
		{
			resource.textureIndex = modelData_.materials[mesh.materialIndex].textureIndex;
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
	// 入力バッファ（SkinnedVertexData）の作成
	size_t inputSize = sizeof(SkinnedVertexData) * totalVertexCount_;
	skinnedVertexInputBuffer_ = modelCommon_->GetDXCommon()->CreateBufferResource(inputSize);

	// 全メッシュの頂点データを統合して書き込み
	SkinnedVertexData* inputData = nullptr;
	skinnedVertexInputBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&inputData));

	uint32_t offset = 0;
	for (const auto& mesh : modelData_.meshes)
	{
		std::memcpy(inputData + offset, mesh.vertices.data(), sizeof(SkinnedVertexData) * mesh.vertices.size());
		offset += static_cast<uint32_t>(mesh.vertices.size());
	}

	// 出力バッファ（変形後VertexData）の作成
	// UAVとして使用するため、D3D12_HEAP_TYPE_DEFAULTと D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESSが必要
	size_t outputSize = sizeof(VertexData) * totalVertexCount_;

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
		D3D12_RESOURCE_STATE_COMMON, // Default HeapはCOMMONで作成する必要がある
		nullptr,
		IID_PPV_ARGS(&skinnedVertexOutputBuffer_)
	);
	assert(SUCCEEDED(hr));

	// 頂点バッファビューを出力バッファに設定
	for (size_t i = 0; i < meshResources_.size(); ++i)
	{
		auto& resource = meshResources_[i];
		resource.vertexBufferView.BufferLocation = skinnedVertexOutputBuffer_->GetGPUVirtualAddress() +
			resource.vertexOffset * sizeof(VertexData);
		resource.vertexBufferView.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * modelData_.meshes[i].vertices.size());
		resource.vertexBufferView.StrideInBytes = sizeof(VertexData);
	}

	// Note: DEFAULT heapはCPUから直接書き込めないため、初期データはコンピュートシェーダーで書き込む
	// 初回のDispatchで入力データをコピーするか、アップロードバッファ経由で初期化が必要
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
