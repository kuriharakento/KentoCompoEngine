#pragma once
#include <d3d12.h>
#include <list>
#include <memory>
#include <wrl.h>

#include "base/GraphicsTypes.h"

class SrvManager;
class DirectXCommon;
class CameraManager;

class ParticleGroup
{
public:
	enum class ParticleType
	{
		Plane,
		Ring,
		Cylinder,
		Sphere,
		Torus,
		Star,
		Heart,
		Spiral,
		Cone,
	};

	ParticleGroup() = default;
	~ParticleGroup();

	void Initialize(const std::string& groupName, const std::string& textureFilePath);
	void Update(CameraManager* camera);
	void Draw(DirectXCommon* dxCommon, SrvManager* srvManager);
	void AddParticle(const Particle& particle) { particles.push_back(particle); }

	void SetTexture(const std::string& textureFilePath);
	void SetModelType(ParticleType type);
	std::list<Particle>& GetParticles() { return particles; }
	void SetBillboard(bool isBillboard) { isBillboard_ = isBillboard; }
	Vector3 GetUVTranslate() const;
	Vector3 GetUVScale() const;
	Vector3 GetUVRotate() const;
	void SetUVTranslate(const Vector3& translate);
	void SetUVScale(const Vector3& scale);
	void SetUVRotate(const Vector3& rotate);
	Vector4 GetMaterialColor() const { return materialData_->color; }
	void SetMaterialColor(const Vector4& color) { materialData_->color = color; }
	uint32_t GetParticleCount() const { return static_cast<uint32_t>(particles.size()); }

private:
	void UpdateInstanceData(Particle& particle, const Matrix4x4& billboardMatrix, CameraManager* camera);
	bool UpdateLifeTime(std::list<Particle>::iterator& itr);
	void UpdateTranslate(std::list<Particle>::iterator& itr);
	void UpdateVertexBuffer(const std::vector<VertexData>& vertices);
	void MakePlaneVertexData();
	void MakeRingVertexData();
	void MakeCylinderVertexData();
	void MakeSphereVertexData();
	void MakeTorusVertexData();
	void MakeStarVertexData();
	void MakeHeartVertexData();
	void MakeSpiralVertexData();
	void MakeConeVertexData();

private:
	//===========================[ 描画設定用変数 ]===========================//
	const uint32_t kMaxParticleCount = 100; // 最大パーティクル数

	MaterialData materialData;
	uint32_t instancingSrvIndex = 0;
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource = nullptr;
	uint32_t instanceCount = 0;
	ParticleForGPU* instancingData = nullptr;
	//モデルの頂点データ
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = nullptr;
	VertexData* vertexData = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	bool isBillboard_ = true; // ビルボードフラグ
	//マテリアルデータ
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_ = nullptr;
	MaterialData modelData_;
	Material* materialData_ = nullptr;

	//===========================[ パーティクル ]===========================//

	std::list<Particle> particles;
};

