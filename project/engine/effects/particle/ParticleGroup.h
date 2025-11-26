#pragma once
#include <d3d12.h>
#include <list>
#include <memory>
#include <wrl.h>

#include "base/GraphicsTypes.h"

class SrvManager;
class DirectXCommon;
class CameraManager;

/**
 * @brief パーティクルグループクラス
 * 
 * パーティクルのインスタンシング描画を管理するクラス。
 * 複数のパーティクルを効率的にGPUへ転送し、一括描画を行う。
 * ビルボード処理やUVアニメーションにも対応。
 */
class ParticleGroup
{
public:
	/**
	 * @brief パーティクルの形状タイプ
	 */
	enum class ParticleType
	{
		Plane,    // 平面（矩形）
		Ring,     // リング（輪）
		Cylinder, // 円柱
		Sphere,   // 球体
		Torus,    // トーラス（ドーナツ形状）
		Star,     // 星型
		Heart,    // ハート型
		Spiral,   // スパイラル（螺旋）
		Cone,     // 円錐
		Cube,     // 立方体
	};

	/**
	 * @brief デフォルトコンストラクタ
	 */
	ParticleGroup() = default;

	/**
	 * @brief デストラクタ
	 */
	~ParticleGroup();

	/**
	 * @brief パーティクルグループを初期化する
	 * @param groupName グループ名
	 * @param textureFilePath テクスチャファイルのパス
	 */
	void Initialize(const std::string& groupName, const std::string& textureFilePath);

	/**
	 * @brief パーティクルを更新する
	 * @param camera カメラマネージャー（ビルボード計算に使用）
	 */
	void Update(CameraManager* camera);

	/**
	 * @brief パーティクルを描画する（インスタンシング描画）
	 * @param dxCommon DirectXCommonインスタンス
	 * @param srvManager SRVマネージャー
	 */
	void Draw(DirectXCommon* dxCommon, SrvManager* srvManager);

	/**
	 * @brief パーティクルを追加する
	 * @param particle 追加するパーティクル
	 */
	void AddParticle(const Particle& particle) { particles.push_back(particle); }

	/**
	 * @brief テクスチャを設定する
	 * @param textureFilePath テクスチャファイルのパス
	 */
	void SetTexture(const std::string& textureFilePath);

	/**
	 * @brief パーティクルのモデルタイプを設定する
	 * @param type パーティクルの形状タイプ
	 */
	void SetModelType(ParticleType type);

	/**
	 * @brief パーティクルリストを取得する
	 * @return パーティクルリストの参照
	 */
	std::list<Particle>& GetParticles() { return particles; }

	/**
	 * @brief ビルボード有効状態を取得する
	 * @return ビルボードが有効ならtrue
	 */
	bool IsBillboard() const { return isBillboard_; }

	/**
	 * @brief ビルボード有効状態を設定する
	 * @param isBillboard ビルボードを有効にするかどうか
	 */
	void SetBillboard(bool isBillboard) { isBillboard_ = isBillboard; }

	/**
	 * @brief UV平行移動値を取得する
	 * @return 現在のUV平行移動値
	 */
	Vector3 GetUVTranslate() const;

	/**
	 * @brief UVスケール値を取得する
	 * @return 現在のUVスケール値
	 */
	Vector3 GetUVScale() const;

	/**
	 * @brief UV回転値を取得する
	 * @return 現在のUV回転値
	 */
	Vector3 GetUVRotate() const;

	/**
	 * @brief UV平行移動値を設定する
	 * @param translate 設定するUV平行移動値
	 */
	void SetUVTranslate(const Vector3& translate);

	/**
	 * @brief UVスケール値を設定する
	 * @param scale 設定するUVスケール値
	 */
	void SetUVScale(const Vector3& scale);

	/**
	 * @brief UV回転値を設定する
	 * @param rotate 設定するUV回転値
	 */
	void SetUVRotate(const Vector3& rotate);

	/**
	 * @brief マテリアルカラーを取得する
	 * @return 現在のマテリアルカラー
	 */
	Vector4 GetMaterialColor() const { return materialData_->color; }

	/**
	 * @brief マテリアルカラーを設定する
	 * @param color 設定するマテリアルカラー
	 */
	void SetMaterialColor(const Vector4& color) { materialData_->color = color; }

	/**
	 * @brief 現在のパーティクル数を取得する
	 * @return パーティクル数
	 */
	uint32_t GetParticleCount() const { return static_cast<uint32_t>(particles.size()); }

private:
	/**
	 * @brief インスタンスデータを更新する
	 * @param particle 更新対象のパーティクル
	 * @param billboardMatrix ビルボード行列
	 * @param camera カメラマネージャー
	 */
	void UpdateInstanceData(Particle& particle, const Matrix4x4& billboardMatrix, CameraManager* camera);

	/**
	 * @brief パーティクルの寿命を更新する
	 * @param itr パーティクルのイテレータ
	 * @return 寿命が切れた場合true
	 */
	bool UpdateLifeTime(std::list<Particle>::iterator& itr);

	/**
	 * @brief パーティクルの位置を更新する
	 * @param itr パーティクルのイテレータ
	 */
	void UpdateTranslate(std::list<Particle>::iterator& itr);

	/**
	 * @brief 頂点バッファを更新する
	 * @param vertices 頂点データ
	 */
	void UpdateVertexBuffer(const std::vector<VertexData>& vertices);

	// 各形状の頂点データ生成関数
	void MakePlaneVertexData();
	void MakeRingVertexData();
	void MakeCylinderVertexData();
	void MakeSphereVertexData();
	void MakeTorusVertexData();
	void MakeStarVertexData();
	void MakeHeartVertexData();
	void MakeSpiralVertexData();
	void MakeConeVertexData();
	void MakeCubeVertexData();

private:
	//===========================[ 描画設定用変数 ]===========================//

	// 最大パーティクル数（GPUインスタンシング用バッファのサイズ）
	static constexpr uint32_t kMaxParticleCount = 100;

	// マテリアルデータ構造体
	MaterialData materialData;
	// インスタンシング用SRVインデックス
	uint32_t instancingSrvIndex = 0;
	// インスタンシング用リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource = nullptr;
	// 描画インスタンス数
	uint32_t instanceCount = 0;
	// GPU転送用インスタンシングデータ
	ParticleForGPU* instancingData = nullptr;
	// 頂点バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = nullptr;
	// 頂点データポインタ
	VertexData* vertexData = nullptr;
	// 頂点バッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	// ビルボードフラグ
	bool isBillboard_ = true;
	// マテリアルリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_ = nullptr;
	// モデルデータ（テクスチャ情報）
	MaterialData modelData_;
	// マテリアルデータポインタ
	Material* materialData_ = nullptr;

	//===========================[ パーティクル ]===========================//

	// パーティクルリスト
	std::list<Particle> particles;
};

