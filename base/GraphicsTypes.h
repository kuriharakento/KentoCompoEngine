#pragma once
#include <string>
#include <vector>
#include <unordered_map>

#include "math/MatrixFunc.h"
#include "math/Vector2.h"
#include "math/Vector4.h"
#include "math/Quaternion.h"

namespace KCE
{
/*==========================================================
 * スキニング用定数
 *==========================================================*/

// 1頂点あたりの最大ボーン影響数
constexpr uint32_t kMaxBoneInfluence = 4;
// モデルあたりの最大ボーン数
constexpr uint32_t kMaxBones = 256;

/**
 * @brief 頂点データ
 */
struct VertexData
{
    // 位置
    Vector4 position;
    // テクスチャ座標
    Vector2 texcoord;
    // 法線
    Vector3 normal;
};

/**
 * @brief 線の頂点データ
 */
struct LineVertex
{
    // 位置
    Vector3 position;
    // 色
    Vector4 color;
    // ワールド座標
	Vector3 worldPos;
};

/**
 * @brief マテリアル
 */
struct Material
{
    // 色
    Vector4 color;
    // ライティングの有効無効
    int32_t enableLighting;
    // パディング（アラインメント用）
    float padding[3];
    // UV変換行列
    Matrix4x4 uvTransform;
    // 反射強度
    float shininess;
    // 反射率
	float reflectivity;
    // パディング（アラインメント用）
    float padding2[2];
};

/**
 * @brief 座標変換行列データ
 */
struct TransformationMatrix
{
    // ワールドビュープロジェクション行列
    Matrix4x4 WVP;
    // ワールド行列
    Matrix4x4 World;
    // ワールド逆転置行列
    Matrix4x4 WorldInverseTranspose;
};

/**
 * @brief ライン用座標変換行列データ
 */
struct LineTransformationMatrix
{
    // ワールドビュープロジェクション行列
	Matrix4x4 WVP;
    // ワールド行列
	Matrix4x4 World;
};

/**
 * @brief トランスフォーム
 */
struct Transform
{
    // スケール
    Vector3 scale;
    // 回転
    Vector3 rotate;
    // 平行移動
    Vector3 translate;

    /**
     * @brief デフォルトコンストラクタ
     */
    Transform()
        : scale({ 1.0f, 1.0f, 1.0f })
        , rotate({ 0.0f, 0.0f, 0.0f })
        , translate({ 0.0f, 0.0f, 0.0f })
    {
    }

    /**
     * @brief パラメータ付きコンストラクタ
     * @param s スケール
     * @param r 回転
     * @param t 平行移動
     */
    Transform(const Vector3& s, const Vector3& r, const Vector3& t)
        : scale(s)
        , rotate(r)
        , translate(t)
    {
	}
};

/**
 * @brief マテリアルデータ
 */
struct MaterialData
{
    // マテリアル名
    std::string name;
    // テクスチャファイルパス
    std::string textureFilePath;
    // テクスチャインデックス
    uint32_t textureIndex = 0;
};

/**
 * @brief メッシュデータ（1つのサブメッシュ）
 */
struct MeshData
{
    // 頂点データ
    std::vector<VertexData> vertices;
    // インデックスデータ
    std::vector<uint32_t> indices;
    // 使用するマテリアルのインデックス
    uint32_t materialIndex = 0;
};

/**
 * @brief ノード
 */
struct Node
{
    // ローカル変換行列
	Matrix4x4 localMatrix;
    // ノード名
	std::string name;
    // 子ノード
	std::vector<Node> children;
};

/**
 * @brief モデルデータ
 */
struct ModelData
{
    // メッシュデータ（マルチメッシュ対応）
    std::vector<MeshData> meshes;
    // マテリアルデータ（マルチマテリアル対応）
    std::vector<MaterialData> materials;
    // ルートノード
	Node rootNode;
};

/**
 * @brief GPU用カメラデータ
 */
struct CameraForGPU
{
    // ワールド座標
    Vector3 worldPos;
    // パディング（アラインメント用）
    float padding;
};

/*==========================================================
 * スキニング用データ構造
 *==========================================================*/

/**
 * @brief スキニング用頂点データ
 */
struct SkinnedVertexData
{
    // 位置
    Vector4 position;
    // テクスチャ座標
    Vector2 texcoord;
    // 法線
    Vector3 normal;
    // 影響するボーンのインデックス
    uint32_t boneIndices[kMaxBoneInfluence];
    // 各ボーンの重み
    float boneWeights[kMaxBoneInfluence];

    SkinnedVertexData()
        : position{ 0, 0, 0, 1 }
        , texcoord{ 0, 0 }
        , normal{ 0, 1, 0 }
    {
        for (uint32_t i = 0; i < kMaxBoneInfluence; ++i)
        {
            boneIndices[i] = 0;
            boneWeights[i] = 0.0f;
        }
    }
};

// HLSLシェーダーとのサイズ一致を確認
static_assert(sizeof(SkinnedVertexData) == 68, "SkinnedVertexData size must be 68 bytes to match HLSL shader");

/**
 * @brief ボーン情報
 */
struct BoneInfo
{
    // ボーン名
    std::string name;
    // 親ボーンのインデックス（-1でルート）
    int32_t parentIndex = -1;
    // オフセット行列（逆バインドポーズ行列）
    Matrix4x4 offsetMatrix;
    // デフォルトローカル変換（バインドポーズ）
    Matrix4x4 defaultLocalTransform;
    
    BoneInfo() : offsetMatrix(MakeIdentity4x4()), defaultLocalTransform(MakeIdentity4x4()) {}
};

/**
 * @brief スケルトン（ボーン階層）
 */
struct Skeleton
{
    // 全ボーン情報
    std::vector<BoneInfo> bones;
    // ボーン名からインデックスへのマップ
    std::unordered_map<std::string, uint32_t> boneNameToIndex;
    // Armatureノードのトランスフォーム行列（スケルトンルートの変換）
    Matrix4x4 armatureTransform = MakeIdentity4x4();

    /**
     * @brief ボーン名からインデックスを取得
     * @param name ボーン名
     * @return ボーンインデックス（見つからない場合は-1）
     */
    int32_t GetBoneIndex(const std::string& name) const
    {
        auto it = boneNameToIndex.find(name);
        if (it != boneNameToIndex.end())
        {
            return static_cast<int32_t>(it->second);
        }
        return -1;
    }
};

/**
 * @brief アニメーションキーフレーム
 */
template <typename T>
struct AnimationKey
{
    float time;
    T value;
};

/**
 * @brief アニメーションチャンネル（1つのボーンのアニメーション）
 */
struct AnimationChannel
{
    // ボーン名
    std::string boneName;
    // ボーンインデックス（ランタイムで設定）
    int32_t boneIndex = -1;
    // 位置キーフレーム
    std::vector<AnimationKey<Vector3>> positionKeys;
    // 回転キーフレーム
    std::vector<AnimationKey<Quaternion>> rotationKeys;
    // スケールキーフレーム
    std::vector<AnimationKey<Vector3>> scaleKeys;
};

/**
 * @brief アニメーションクリップ
 */
struct AnimationClip
{
    // アニメーション名
    std::string name;
    // アニメーションの長さ（秒）
    float duration = 0.0f;
    // ティック/秒
    float ticksPerSecond = 25.0f;
    // 各ボーンのアニメーションチャンネル
    std::vector<AnimationChannel> channels;
};

/**
 * @brief スキニング用メッシュデータ
 */
struct SkinnedMeshData
{
    // スキニング用頂点データ
    std::vector<SkinnedVertexData> vertices;
    // インデックスデータ
    std::vector<uint32_t> indices;
    // 使用するマテリアルのインデックス
    uint32_t materialIndex = 0;
};

/**
 * @brief スキニング用モデルデータ
 */
struct SkinnedModelData
{
    // スキニング用メッシュデータ
    std::vector<SkinnedMeshData> meshes;
    // マテリアルデータ
    std::vector<MaterialData> materials;
    // スケルトン
    Skeleton skeleton;
    // アニメーションクリップ
    std::vector<AnimationClip> animations;
    // ルートノード
    Node rootNode;
};
} // namespace KCE
