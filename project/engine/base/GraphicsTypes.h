#pragma once
#include <string>
#include <vector>

#include "math/MatrixFunc.h"
#include "math/Vector2.h"
#include "math/Vector4.h"

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
    // テクスチャファイルパス
    std::string textureFilePath;
    // テクスチャインデックス
    uint32_t textureIndex = 0;
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
    // 頂点データ
	std::vector<VertexData> vertices;
    // マテリアルデータ
	MaterialData material;
    // ルートノード
	Node rootNode;
};

/**
 * @brief パーティクル
 */
struct Particle
{
    // トランスフォーム
    Transform transform;
    // 速度
    Vector3 velocity;
    // 色
    Vector4 color;
    // 寿命
    float lifeTime;
    // 現在の時間
    float currentTime;
    // 開始位置
	Vector3 startPos;

    /**
     * @brief パーティクルが生存しているかを判定
     * @return 生存している場合はtrue
     */
	bool isAlive() const { return currentTime < lifeTime; }
};

/**
 * @brief GPU用パーティクルデータ
 */
struct ParticleForGPU
{
    // ワールドビュープロジェクション行列
    Matrix4x4 WVP;
    // ワールド行列
    Matrix4x4 World;
    // 色
    Vector4 color;
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
