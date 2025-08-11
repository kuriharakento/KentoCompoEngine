#include "ParticleGroup.h"

#include <algorithm>
#include <numbers>

// math
#include "ParticleMath.h"
#include "math/MathUtils.h"
// system
#include "manager/scene/CameraManager.h"
#include "manager/system/SrvManager.h"
#include "base/DirectXCommon.h"
#include "base/Logger.h"
#include "manager/graphics/TextureManager.h"
#include "manager/effect/ParticleManager.h"
#include "time/TimeManager.h"


ParticleGroup::~ParticleGroup()
{
	// リソースの解放
	if (instancingResource)
	{
		instancingResource->Unmap(0, nullptr);
		instancingResource.Reset();
		instancingData = nullptr;
	}
	if (vertexResource)
	{
		vertexResource.Reset();
		vertexData = nullptr;
	}
	if (materialResource_)
	{
		materialResource_->Unmap(0, nullptr);
		materialResource_.Reset();
		materialData_ = nullptr;
	}
	particles.clear();
}

void ParticleGroup::Initialize(const std::string& groupName, const std::string& textureFilePath)
{
	// 各種リソースの初期化
	// テクスチャの読み込み
	modelData_.textureFilePath = textureFilePath;
	TextureManager::GetInstance()->LoadTexture(modelData_.textureFilePath);
	modelData_.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(modelData_.textureFilePath);

	//マテリアルリソース
	materialResource_ = ParticleManager::GetInstance()->GetDxCommon()->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->uvTransform = MakeIdentity4x4();
	materialData_->enableLighting = false;

	// 頂点バッファの初期化
	std::vector<VertexData> rectangleVertices = {
		{ {  1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }, // 右上
		{ { -1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }, // 左上
		{ {  1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } }, // 右下
		{ {  1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } }, // 右下
		{ { -1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }, // 左上
		{ { -1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } }  // 左下
	};
	vertexResource = ParticleManager::GetInstance()->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * rectangleVertices.size());
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, rectangleVertices.data(), sizeof(VertexData) * rectangleVertices.size());
	vertexResource->Unmap(0, nullptr);

	// 頂点バッファービューの初期化
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.StrideInBytes = sizeof(VertexData);
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * rectangleVertices.size());

	// インスタンシング用リソースの初期化
	instancingResource = ParticleManager::GetInstance()->GetDxCommon()->CreateBufferResource(sizeof(ParticleForGPU) * kMaxParticleCount);
	instancingResource->Map(0, nullptr, reinterpret_cast<void**>(&instancingData));
	instancingSrvIndex = ParticleManager::GetInstance()->GetSrvManager()->Allocate();
	// SRVの生成
	ParticleManager::GetInstance()->GetSrvManager()->CreateSRVforStructuredBuffer(
		instancingSrvIndex,
		instancingResource.Get(),
		kMaxParticleCount, // numElements: パーティクルの最大数
		sizeof(ParticleForGPU) // structureByteStride: 各パーティクルのサイズ
	);
}

void ParticleGroup::Update(CameraManager* camera)
{
	if (particles.empty()) { return; } // パーティクルがない場合は更新しない

	float kDeltaTime = TimeManager::GetInstance().GetDeltaTime();

	// ビルボード用の行列計算
	Matrix4x4 backToFrontMatrix = MakeRotateYMatrix(std::numbers::pi_v<float>); // Z軸正方向を基準にする
	Matrix4x4 billboardMatrix = MakeIdentity4x4();

	// カメラの回転を取得
	Matrix4x4 cameraRotationMatrix = camera->GetActiveCamera()->GetWorldMatrix();
	cameraRotationMatrix.m[3][0] = 0.0f;
	cameraRotationMatrix.m[3][1] = 0.0f;
	cameraRotationMatrix.m[3][2] = 0.0f;

	// カメラの回転をビルボード行列に適用
	billboardMatrix = backToFrontMatrix * cameraRotationMatrix;

	instanceCount = 0; // このグループのインスタンスカウントをリセット

	for (auto particleItr = particles.begin(); particleItr != particles.end(); )
	{
		// 寿命の更新
		if (UpdateLifeTime(particleItr))
		{
			// 寿命が切れたパーティクルを削除
			particleItr = particles.erase(particleItr);
			continue;
		}

		if (instanceCount < kMaxParticleCount)
		{
			// 座標を速度によって更新
			UpdateTranslate(particleItr);
			// インスタンスデータの更新
			UpdateInstanceData(*particleItr, billboardMatrix, camera);

			++instanceCount;
		}
		++particleItr;
	}
}


void ParticleGroup::Draw(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	// 頂点数を計算
	UINT vertexCount = static_cast<UINT>(vertexBufferView.SizeInBytes / vertexBufferView.StrideInBytes);
	// インスタンスがない場合は描画しない
	if (instanceCount == 0) { return; }

	//描画設定
	dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(1, srvManager->GetGPUDescriptorHandle(instancingSrvIndex));
	dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, srvManager->GetGPUDescriptorHandle(modelData_.textureIndex));
	// インスタンシング描画
	dxCommon->GetCommandList()->DrawInstanced(vertexCount, instanceCount, 0, 0);
}

void ParticleGroup::SetTexture(const std::string& textureFilePath)
{
	modelData_.textureFilePath = textureFilePath;
	TextureManager::GetInstance()->LoadTexture(modelData_.textureFilePath);
	modelData_.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(modelData_.textureFilePath);

}

void ParticleGroup::SetModelType(ParticleType type)
{
	switch (type)
	{
	case ParticleType::Plane:
		MakePlaneVertexData();
		break;
	case ParticleType::Ring:
		MakeRingVertexData();
		break;
	case ParticleType::Cylinder:
		MakeCylinderVertexData();
		break;
	case ParticleType::Sphere:
		MakeSphereVertexData();
		break;
	case ParticleType::Torus:
		MakeTorusVertexData();
		break;
	case ParticleType::Star:
		MakeStarVertexData();
		break;
	case ParticleType::Heart:
		MakeHeartVertexData();
		break;
	case ParticleType::Spiral:
		MakeSpiralVertexData();
		break;
	case ParticleType::Cone:
		MakeConeVertexData();
		break;
	default:
		Logger::Log("Invalid particle type.");
		assert(false);
		break;
	}
}

Vector3 ParticleGroup::GetUVTranslate() const
{
	return MathUtils::GetMatrixTranslate(materialData_->uvTransform);
}

Vector3 ParticleGroup::GetUVScale() const
{
	return MathUtils::GetMatrixScale(materialData_->uvTransform);
}

Vector3 ParticleGroup::GetUVRotate() const
{
	return MathUtils::GetMatrixRotate(materialData_->uvTransform);
}

void ParticleGroup::SetUVTranslate(const Vector3& translate)
{
	materialData_->uvTransform = MakeAffineMatrix(GetUVScale(), GetUVRotate(), translate);
}

void ParticleGroup::SetUVScale(const Vector3& scale)
{
	materialData_->uvTransform = MakeAffineMatrix(scale, GetUVRotate(), GetUVTranslate());
}

void ParticleGroup::SetUVRotate(const Vector3& rotate)
{
	materialData_->uvTransform = MakeAffineMatrix(GetUVScale(), rotate, GetUVTranslate());
}

void ParticleGroup::UpdateInstanceData(Particle& particle, const Matrix4x4& billboardMatrix, CameraManager* camera)
{
	// スケール、回転、平行移動の行列を計算
	Matrix4x4 scaleMatrix = MakeScaleMatrix(particle.transform.scale);
	Matrix4x4 rotateMatrix = MakeRotateMatrix(particle.transform.rotate);
	Matrix4x4 translateMatrix = MakeTranslateMatrix(particle.transform.translate);
	// ビルボード適用
	Matrix4x4 worldMatrixInstancing = scaleMatrix * rotateMatrix;
	if (isBillboard_)
	{
		worldMatrixInstancing = worldMatrixInstancing * billboardMatrix;
	}
	worldMatrixInstancing = worldMatrixInstancing * translateMatrix;
	// WVP行列計算
	Matrix4x4 wvp = Multiply(worldMatrixInstancing,
							 Multiply(camera->GetActiveCamera()->GetViewMatrix(),
									  camera->GetActiveCamera()->GetProjectionMatrix()));
	// インスタンシング用データにセット
	if (instancingData)
	{
		instancingData[instanceCount].World = worldMatrixInstancing;
		instancingData[instanceCount].WVP = wvp;
		instancingData[instanceCount].color = particle.color;
	}
}

bool ParticleGroup::UpdateLifeTime(std::list<Particle>::iterator& itr)
{
	// 寿命を更新
	itr->currentTime += TimeManager::GetInstance().GetDeltaTime();
	// 寿命が切れたらtrueを返す
	if (itr->currentTime >= itr->lifeTime)
	{
		return true;
	}
	return false;
}

void ParticleGroup::UpdateTranslate(std::list<Particle>::iterator& itr)
{
	// 速度を加算
	itr->transform.translate += itr->velocity * TimeManager::GetInstance().GetDeltaTime(); // 1フレーム分の時間を加算
}

void ParticleGroup::UpdateVertexBuffer(const std::vector<VertexData>& vertices)
{
	// 古いリソースを明示的に解放
	if (vertexResource)
	{
		vertexResource.Reset();
		vertexData = nullptr;
	}

	// 頂点データをGPUへ転送
	vertexResource = ParticleManager::GetInstance()->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * vertices.size());
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, vertices.data(), sizeof(VertexData) * vertices.size());
	vertexResource->Unmap(0, nullptr);
	// 頂点バッファービューの再設定
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.StrideInBytes = sizeof(VertexData);
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * vertices.size());
}

void ParticleGroup::MakePlaneVertexData()
{
	// 頂点データを矩形で初期化
	std::vector<VertexData> rectangleVertices = ParticleMath::MakePlaneVertexData();

	// 頂点データを更新
	UpdateVertexBuffer(rectangleVertices);
}

void ParticleGroup::MakeRingVertexData()
{
	// 頂点データをリングで初期化
	std::vector<VertexData> ringVertices = ParticleMath::MakeRingVertexData();

	// 頂点データを更新
	UpdateVertexBuffer(ringVertices);
}

void ParticleGroup::MakeCylinderVertexData()
{
	// 頂点データを円柱で初期化
	std::vector<VertexData> cylinderVertices = ParticleMath::MakeCylinderVertexData();

	// 頂点データを更新
	UpdateVertexBuffer(cylinderVertices);
}

void ParticleGroup::MakeSphereVertexData()
{
	// 頂点データを球体で初期化
	std::vector<VertexData> sphereVertices = ParticleMath::MakeSphereVertexData();

	// 頂点データを更新
	UpdateVertexBuffer(sphereVertices);
}

void ParticleGroup::MakeTorusVertexData()
{
	// 頂点データをトーラスで初期化
	std::vector<VertexData> torusVertices = ParticleMath::MakeTorusVertexData();

	// 頂点データを更新
	UpdateVertexBuffer(torusVertices);
}

void ParticleGroup::MakeStarVertexData()
{
	// 頂点データを星型で初期化
	std::vector<VertexData> vertices = ParticleMath::MakeStarVertexData();

	// 頂点データを更新
	UpdateVertexBuffer(vertices);
}

void ParticleGroup::MakeHeartVertexData()
{
	// 頂点データをハート型で初期化
	std::vector<VertexData> vertices = ParticleMath::MakeHeartVertexData();

	// 頂点データを更新
	UpdateVertexBuffer(vertices);
}

void ParticleGroup::MakeSpiralVertexData()
{
	// 頂点データをスパイラルで初期化
	std::vector<VertexData> spiralVertices = ParticleMath::MakeSpiralVertexData();
	// 頂点データを更新
	UpdateVertexBuffer(spiralVertices);
}

void ParticleGroup::MakeConeVertexData()
{
	// 頂点データを円錐で初期化
	std::vector<VertexData> coneVertices = ParticleMath::MakeConeVertexData();
	// 頂点データを更新
	UpdateVertexBuffer(coneVertices);
}
