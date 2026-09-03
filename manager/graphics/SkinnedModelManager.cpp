#include "SkinnedModelManager.h"
#include "base/DirectXCommon.h"
#include "base/Logger.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <cassert>
#include <Windows.h>
#include <functional>
#include <cmath>
#include <cstring>

#include "base/PathManager.h"
#include "manager/graphics/TextureManager.h"

namespace KCE
{
// 定数
constexpr int kMatrixRows = 4;
constexpr int kMatrixColumns = 4;
constexpr float kLeftHandConversion = -1.0f;
constexpr float kVertexW = 1.0f;
const std::string kDefaultTexturePath = "textures/white1x1.png";

std::unique_ptr<SkinnedModelManager> SkinnedModelManager::instance_ = nullptr;

SkinnedModelManager* SkinnedModelManager::GetInstance()
{
    if (!instance_)
    {
		instance_ = std::make_unique<SkinnedModelManager>();
    }
    return instance_.get();
}

void SkinnedModelManager::Initialize(DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;
}

void SkinnedModelManager::Finalize()
{
    modelCache_.clear();
    instance_.reset();
}

const SkinnedModelSharedResource* SkinnedModelManager::LoadModel(const std::string& directoryPath, const std::string& filename, const std::string& modelType)
{
    std::string relativeModelPath = directoryPath + "/" + filename + "/" + filename + modelType;
    std::string fullPath = relativeModelPath;
    if (!std::filesystem::exists(fullPath))
    {
        std::filesystem::path resolved = PathManager::ResolveApplicationResource(relativeModelPath);
        if (std::filesystem::exists(resolved))
        {
            fullPath = resolved.string();
        }
        else
        {
            std::filesystem::path appModels = PathManager::GetApplicationResourceRoot() / "models";
            std::vector<std::filesystem::path> searchPaths = {
                appModels / filename / (filename + modelType),
                appModels / (filename + modelType)
            };
            for (const auto& path : searchPaths)
            {
                if (std::filesystem::exists(path))
                {
                    fullPath = path.string();
                    break;
                }
            }
        }
    }
    
    // キャッシュ確認
    auto it = modelCache_.find(fullPath);
    if (it != modelCache_.end())
    {
        return it->second.get();
    }

    // 新規ロード
    auto sharedResource = std::make_unique<SkinnedModelSharedResource>();
    
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(fullPath.c_str(),
        aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_Triangulate |
        aiProcess_LimitBoneWeights | aiProcess_JoinIdenticalVertices);

    if (!scene || !scene->HasMeshes())
    {
        KCE::Logger::Log("Failed to load skinned model: " + fullPath + "\n");
        return nullptr;
    }

    // 1. モデルデータの抽出 (SkinnedModel から移植した関数群を内部的に呼ぶイメージ)
    // --- ExtractBones ---
    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
    {
        aiMesh* mesh = scene->mMeshes[meshIndex];
        for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
        {
            aiBone* bone = mesh->mBones[boneIndex];
            std::string boneName = bone->mName.C_Str();
            if (sharedResource->modelData.skeleton.boneNameToIndex.find(boneName) == sharedResource->modelData.skeleton.boneNameToIndex.end())
            {
                BoneInfo boneInfo;
                boneInfo.name = boneName;
                aiMatrix4x4 aiOffsetMatrix = bone->mOffsetMatrix;
                aiOffsetMatrix.Transpose();
                for (int r = 0; r < 4; ++r)
                    for (int c = 0; c < 4; ++c)
                        boneInfo.offsetMatrix.m[r][c] = aiOffsetMatrix[r][c];

                // 左手系変換
                boneInfo.offsetMatrix.m[0][1] *= -1.0f;
                boneInfo.offsetMatrix.m[0][2] *= -1.0f;
                boneInfo.offsetMatrix.m[1][0] *= -1.0f;
                boneInfo.offsetMatrix.m[2][0] *= -1.0f;
                boneInfo.offsetMatrix.m[3][0] *= -1.0f;

                uint32_t newIdx = (uint32_t)sharedResource->modelData.skeleton.bones.size();
                sharedResource->modelData.skeleton.boneNameToIndex[boneName] = newIdx;
                sharedResource->modelData.skeleton.bones.push_back(boneInfo);
            }
        }
    }

    // --- Armature 検索 ---
    if (scene->mRootNode)
    {
        aiNode* armatureNode = nullptr;
        aiVector3D rootScaling, rootPosition;
        aiQuaternion rootRotation;
        scene->mRootNode->mTransformation.Decompose(rootScaling, rootRotation, rootPosition);
        if (std::abs(rootScaling.x - 1.0f) > 0.001f || std::abs(rootScaling.y - 1.0f) > 0.001f || std::abs(rootScaling.z - 1.0f) > 0.001f)
            armatureNode = scene->mRootNode;
        else
        {
            for (uint32_t i = 0; i < scene->mRootNode->mNumChildren; ++i)
            {
                aiNode* child = scene->mRootNode->mChildren[i];
                aiVector3D s, p; aiQuaternion r;
                child->mTransformation.Decompose(s,r,p);
                if (std::abs(s.x-1.0f)>0.001f || std::abs(s.y-1.0f)>0.001f || std::abs(s.z-1.0f)>0.001f)
                { armatureNode = child; break; }
            }
            if (!armatureNode)
            {
                for (uint32_t i = 0; i < scene->mRootNode->mNumChildren; ++i)
                {
                    aiNode* child = scene->mRootNode->mChildren[i];
                    bool hasBoneChild = false;
                    for (uint32_t j = 0; j < child->mNumChildren; ++j)
                    {
                        if (sharedResource->modelData.skeleton.GetBoneIndex(child->mChildren[j]->mName.C_Str()) >= 0)
                        { hasBoneChild = true; break; }
                    }
                    if (hasBoneChild || std::string(child->mName.C_Str()).find("Armature") != std::string::npos)
                    { armatureNode = child; break; }
                }
            }
        }
        if (armatureNode)
        {
            aiMatrix4x4 aiArm = armatureNode->mTransformation;
            aiArm.Transpose();
            for (int r=0; r<4; ++r) for (int c=0; c<4; ++c) sharedResource->modelData.skeleton.armatureTransform.m[r][c] = aiArm[r][c];
            sharedResource->modelData.skeleton.armatureTransform.m[0][1]*=-1.f;
            sharedResource->modelData.skeleton.armatureTransform.m[0][2]*=-1.f;
            sharedResource->modelData.skeleton.armatureTransform.m[1][0]*=-1.f;
            sharedResource->modelData.skeleton.armatureTransform.m[2][0]*=-1.f;
            sharedResource->modelData.skeleton.armatureTransform.m[3][0]*=-1.f;
        }
    }

    // --- 親子関係 ---
    std::function<void(aiNode*, int32_t)> findParents = [&](aiNode* node, int32_t parentIndex)
    {
        int32_t currentBoneIndex = sharedResource->modelData.skeleton.GetBoneIndex(node->mName.C_Str());
        if (currentBoneIndex >= 0)
        {
            sharedResource->modelData.skeleton.bones[currentBoneIndex].parentIndex = parentIndex;
            aiMatrix4x4 aiLocal = node->mTransformation; aiLocal.Transpose();
            Matrix4x4 local;
            for(int r=0; r<4; ++r) for(int c=0; c<4; ++c) local.m[r][c] = aiLocal[r][c];
            local.m[0][1]*=-1.f; local.m[0][2]*=-1.f; local.m[1][0]*=-1.f; local.m[2][0]*=-1.f; local.m[3][0]*=-1.f;
            sharedResource->modelData.skeleton.bones[currentBoneIndex].defaultLocalTransform = local;
            parentIndex = currentBoneIndex;
        }
        for (uint32_t i=0; i<node->mNumChildren; ++i) findParents(node->mChildren[i], parentIndex);
    };
    if (scene->mRootNode) findParents(scene->mRootNode, -1);

    // --- マテリアル ---
    for (uint32_t i=0; i<scene->mNumMaterials; ++i)
    {
        aiMaterial* aiMat = scene->mMaterials[i];
        MaterialData mat;
        aiString name; if(aiMat->Get(AI_MATKEY_NAME, name)==AI_SUCCESS) mat.name = name.C_Str();
        if(aiMat->GetTextureCount(aiTextureType_DIFFUSE)>0)
        { aiString path; aiMat->GetTexture(aiTextureType_DIFFUSE,0,&path); mat.textureFilePath = path.C_Str(); }
		if(aiMat->GetTextureCount(aiTextureType_EMISSIVE)>0)
		{ aiString path; aiMat->GetTexture(aiTextureType_EMISSIVE,0,&path); mat.emissiveTextureFilePath = path.C_Str(); }
        sharedResource->modelData.materials.push_back(mat);
    }
    std::string basePath = directoryPath + "/" + filename + "/";
    for (auto& mat : sharedResource->modelData.materials)
    {
        std::string fullTex = mat.textureFilePath.empty() ? kDefaultTexturePath : basePath + mat.textureFilePath;
        TextureManager::GetInstance()->LoadTexture(fullTex);
        mat.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(fullTex);
		std::string fullEmissive = mat.emissiveTextureFilePath.empty() || !TextureManager::GetInstance()->CheckTextureExists(basePath + mat.emissiveTextureFilePath)
			? "./Resources/textures/emissive_black_1x1.png" : basePath + mat.emissiveTextureFilePath;
		TextureManager::GetInstance()->LoadTextureLinear(fullEmissive);
		mat.emissiveTextureIndex = TextureManager::GetInstance()->GetLinearTextureIndexByFilePath(fullEmissive);
    }

    // --- メッシュ ---
    for (uint32_t i=0; i<scene->mNumMeshes; ++i)
    {
        aiMesh* mesh = scene->mMeshes[i];
        SkinnedMeshData md; md.materialIndex = mesh->mMaterialIndex;
        md.vertices.resize(mesh->mNumVertices);
        for (uint32_t v=0; v<mesh->mNumVertices; ++v)
        {
            md.vertices[v].position = { mesh->mVertices[v].x*kLeftHandConversion, mesh->mVertices[v].y, mesh->mVertices[v].z, kVertexW };
            if (mesh->HasNormals()) md.vertices[v].normal = { mesh->mNormals[v].x*kLeftHandConversion, mesh->mNormals[v].y, mesh->mNormals[v].z };
            if (mesh->HasTextureCoords(0)) md.vertices[v].texcoord = { mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y };
        }
        for (uint32_t f=0; f<mesh->mNumFaces; ++f)
        { assert(mesh->mFaces[f].mNumIndices==3); for(int idx=0; idx<3; ++idx) md.indices.push_back(mesh->mFaces[f].mIndices[idx]); }
        
        // Weights
        for (uint32_t b=0; b<mesh->mNumBones; ++b)
        {
            std::string bname = mesh->mBones[b]->mName.C_Str();
            int32_t skBoneIdx = sharedResource->modelData.skeleton.GetBoneIndex(bname);
            if (skBoneIdx < 0) continue;
            for (uint32_t w=0; w<mesh->mBones[b]->mNumWeights; ++w)
            {
                uint32_t vid = mesh->mBones[b]->mWeights[w].mVertexId;
                float weight = mesh->mBones[b]->mWeights[w].mWeight;
                for (int slot=0; slot<4; ++slot) { if(md.vertices[vid].boneWeights[slot]==0.f){ md.vertices[vid].boneIndices[slot]=skBoneIdx; md.vertices[vid].boneWeights[slot]=weight; break; } }
            }
        }
        // Normalize
        for (auto& v : md.vertices)
        {
            float tw=0; for(int j=0; j<4; ++j) tw+=v.boneWeights[j];
            if(tw>0.f){ for(int j=0; j<4; ++j) v.boneWeights[j]/=tw; }
            else { v.boneIndices[0]=0; v.boneWeights[0]=1.f; }
        }
        sharedResource->modelData.meshes.push_back(md);
    }

    // --- Animations ---
    for (uint32_t i=0; i<scene->mNumAnimations; ++i)
    {
        aiAnimation* aiAnim = scene->mAnimations[i];
        AnimationClip clip; clip.name = aiAnim->mName.C_Str();
        clip.ticksPerSecond = (float)(aiAnim->mTicksPerSecond > 0 ? aiAnim->mTicksPerSecond : 25.0);
        clip.duration = (float)(aiAnim->mDuration / clip.ticksPerSecond);
        for(uint32_t c=0; c<aiAnim->mNumChannels; ++c)
        {
            aiNodeAnim* na = aiAnim->mChannels[c]; AnimationChannel chan; chan.boneName = na->mNodeName.C_Str();
            chan.boneIndex = sharedResource->modelData.skeleton.GetBoneIndex(chan.boneName);
            for(uint32_t k=0; k<na->mNumPositionKeys; ++k)
                chan.positionKeys.push_back({(float)(na->mPositionKeys[k].mTime/clip.ticksPerSecond), {na->mPositionKeys[k].mValue.x*kLeftHandConversion, na->mPositionKeys[k].mValue.y, na->mPositionKeys[k].mValue.z}});
            for(uint32_t k=0; k<na->mNumRotationKeys; ++k)
                chan.rotationKeys.push_back({(float)(na->mRotationKeys[k].mTime/clip.ticksPerSecond), Quaternion(na->mRotationKeys[k].mValue.x, -na->mRotationKeys[k].mValue.y, -na->mRotationKeys[k].mValue.z, na->mRotationKeys[k].mValue.w)});
            for(uint32_t k=0; k<na->mNumScalingKeys; ++k)
                chan.scaleKeys.push_back({(float)(na->mScalingKeys[k].mTime/clip.ticksPerSecond), {na->mScalingKeys[k].mValue.x, na->mScalingKeys[k].mValue.y, na->mScalingKeys[k].mValue.z}});
            clip.channels.push_back(chan);
        }
        sharedResource->modelData.animations.push_back(clip);
    }

    // --- Node Tree ---
    std::function<Node(aiNode*)> readNode = [&](aiNode* node) -> Node
    {
        Node res; res.name = node->mName.C_Str();
        aiMatrix4x4 aiT = node->mTransformation; aiT.Transpose();
        for(int r=0; r<4; ++r) for(int c=0; c<4; ++c) res.localMatrix.m[r][c] = aiT[r][c];
        for(uint32_t i=0; i<node->mNumChildren; ++i) res.children.push_back(readNode(node->mChildren[i]));
        return res;
    };
    if (scene->mRootNode) sharedResource->modelData.rootNode = readNode(scene->mRootNode);

    // 2. GPU 静的リソースの作成
    sharedResource->totalVertexCount = 0;
    for (const auto& m : sharedResource->modelData.meshes) sharedResource->totalVertexCount += (uint32_t)m.vertices.size();

    // 入力頂点バッファ
    size_t inSize = sizeof(SkinnedVertexData) * sharedResource->totalVertexCount;
    sharedResource->inputVertexBuffer = dxCommon_->CreateBufferResource(inSize);
    SkinnedVertexData* inPtr; sharedResource->inputVertexBuffer->Map(0, nullptr, (void**)&inPtr);
    uint32_t offset = 0;
    for (const auto& m : sharedResource->modelData.meshes)
    {
        std::memcpy(inPtr + offset, m.vertices.data(), sizeof(SkinnedVertexData) * m.vertices.size());
        
        sharedResource->meshes.push_back({ nullptr, (uint32_t)m.indices.size(), (uint32_t)m.vertices.size(), offset });
        auto& meshRes = sharedResource->meshes.back();
        size_t idxSize = sizeof(uint32_t) * m.indices.size();
        meshRes.indexBuffer = dxCommon_->CreateBufferResource(idxSize);
        uint32_t* idxPtr; meshRes.indexBuffer->Map(0, nullptr, (void**)&idxPtr);
        std::memcpy(idxPtr, m.indices.data(), idxSize);
        meshRes.indexBuffer->Unmap(0, nullptr);

        offset += (uint32_t)m.vertices.size();
    }
    sharedResource->inputVertexBuffer->Unmap(0, nullptr);

    // 不要になった CPU 側の頂点・インデックスデータを破棄してメモリを節約
    for (auto& m : sharedResource->modelData.meshes)
    {
        m.vertices.clear();
        m.vertices.shrink_to_fit();
        m.indices.clear();
        m.indices.shrink_to_fit();
    }

    modelCache_[fullPath] = std::move(sharedResource);
    return modelCache_[fullPath].get();
}
} // namespace KCE
