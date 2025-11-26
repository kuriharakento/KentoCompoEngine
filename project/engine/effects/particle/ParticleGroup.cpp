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
	// インスタンシング用リソースの解放
	if (instancingResource)
	{
		instancingResource->Unmap(0, nullptr);
		instancingResource.Reset();
		instancingData = nullptr;
	}
	// 頂点バッファリソースの解放
	if (vertexResource)
	{
		vertexResource.Reset();
		vertexData = nullptr;
	}
	// マテリアルリソースの解放
	if (materialResource_)
	{
		materialResource_->Unmap(0, nullptr);
		materialResource_.Reset();
		materialData_ = nullptr;
	}
	// パーティクルリストをクリア
	particles.clear();
}

void ParticleGroup::Initialize(const std::string& groupName, const std::string& textureFilePath)
{
	// テクスチャの読み込みとインデックス取得
	modelData_.textureFilePath = textureFilePath;
	TextureManager::GetInstance()->LoadTexture(modelData_.textureFilePath);
	modelData_.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(modelData_.textureFilePath);

	// マテリアルリソースの作成とマッピング
	materialResource_ = ParticleManager::GetInstance()->GetDxCommon()->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	// マテリアルの初期値を設定
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->uvTransform = MakeIdentity4x4();
	materialData_->enableLighting = false;

	// 頂点バッファの初期化（デフォルトは矩形）
	std::vector<VertexData> rectangleVertices = {
		{ {  1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }, // 右上
		{ { -1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }, // 左上
		{ {  1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } }, // 右下
		{ {  1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } }, // 右下
		{ { -1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }, // 左上
		{ { -1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } }  // 左下
	};
	// 頂点バッファリソースの作成とデータ転送
	vertexResource = ParticleManager::GetInstance()->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * rectangleVertices.size());
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, rectangleVertices.data(), sizeof(VertexData) * rectangleVertices.size());
	vertexResource->Unmap(0, nullptr);

	// 頂点バッファービューの設定
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.StrideInBytes = sizeof(VertexData);
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * rectangleVertices.size());

	// インスタンシング用リソースの作成（GPU定数バッファ）
	instancingResource = ParticleManager::GetInstance()->GetDxCommon()->CreateBufferResource(sizeof(ParticleForGPU) * kMaxParticleCount);
	instancingResource->Map(0, nullptr, reinterpret_cast<void**>(&instancingData));
	// SRVインデックスを確保
	instancingSrvIndex = ParticleManager::GetInstance()->GetSrvManager()->Allocate();
	// インスタンシング用StructuredBuffer用のSRVを生成
	ParticleManager::GetInstance()->GetSrvManager()->CreateSRVforStructuredBuffer(
		instancingSrvIndex,
		instancingResource.Get(),
		kMaxParticleCount, // numElements: パーティクルの最大数
		sizeof(ParticleForGPU) // structureByteStride: 各パーティクルのサイズ（GPUアラインメント考慮）
	);
}

void ParticleGroup::Update(CameraManager* camera)
{
	// パーティクルがない場合は更新しない
	if (particles.empty()) { return; }

	// デルタタイムを取得（未使用だが将来の拡張用）
	float kDeltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;

	// ビルボード用の行列を計算
	// Z軸正方向を基準にするため180度回転
	Matrix4x4 backToFrontMatrix = MakeRotateYMatrix(std::numbers::pi_v<float>);
	Matrix4x4 billboardMatrix = MakeIdentity4x4();

	// カメラの回転行列を取得（平行移動成分を除去）
	Matrix4x4 cameraRotationMatrix = camera->GetActiveCamera()->GetWorldMatrix();
	cameraRotationMatrix.m[3][0] = 0.0f;
	cameraRotationMatrix.m[3][1] = 0.0f;
	cameraRotationMatrix.m[3][2] = 0.0f;

	// ビルボード行列を計算（カメラに向く回転）
	billboardMatrix = backToFrontMatrix * cameraRotationMatrix;

	// このフレームのインスタンスカウントをリセット
	instanceCount = 0;

	// 各パーティクルを更新
	for (auto particleItr = particles.begin(); particleItr != particles.end(); )
	{
		// 寿命を更新し、切れていたら削除
		if (UpdateLifeTime(particleItr))
		{
			particleItr = particles.erase(particleItr);
			continue;
		}

		// 最大パーティクル数を超えないように制限
		if (instanceCount < kMaxParticleCount)
		{
			// 速度に基づいて位置を更新
			UpdateTranslate(particleItr);
			// GPU転送用のインスタンスデータを更新
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

	// 頂点バッファを設定
	dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);
	// マテリアル定数バッファを設定
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	// インスタンシング用StructuredBufferを設定
	dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(1, srvManager->GetGPUDescriptorHandle(instancingSrvIndex));
	// テクスチャを設定
	dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, srvManager->GetGPUDescriptorHandle(modelData_.textureIndex));
	// インスタンシング描画を実行
	dxCommon->GetCommandList()->DrawInstanced(vertexCount, instanceCount, 0, 0);
}

void ParticleGroup::SetTexture(const std::string& textureFilePath)
{
	// テクスチャパスを設定
	modelData_.textureFilePath = textureFilePath;
	// テクスチャを読み込み
	TextureManager::GetInstance()->LoadTexture(modelData_.textureFilePath);
	// テクスチャインデックスを取得
	modelData_.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(modelData_.textureFilePath);
}

void ParticleGroup::SetModelType(ParticleType type)
{
	// 形状タイプに応じた頂点データを生成
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
	case ParticleType::Cube:
		MakeCubeVertexData();
		break;
	default:
		// 無効な形状タイプの場合はエラー
		Logger::Log("Invalid particle type.");
		assert(false);
		break;
	}
}

Vector3 ParticleGroup::GetUVTranslate() const
{
	// UV変換行列から平行移動成分を抽出
	return MathUtils::GetTranslateFromMatrix(materialData_->uvTransform);
}

Vector3 ParticleGroup::GetUVScale() const
{
	// UV変換行列からスケール成分を抽出
	return MathUtils::GetScaleFromMatrix(materialData_->uvTransform);
}

Vector3 ParticleGroup::GetUVRotate() const
{
	// UV変換行列から回転成分を抽出
	return MathUtils::GetRotateFromMatrix(materialData_->uvTransform);
}

void ParticleGroup::SetUVTranslate(const Vector3& translate)
{
	// 新しい平行移動値でUV変換行列を再構築
	materialData_->uvTransform = MakeAffineMatrix(GetUVScale(), GetUVRotate(), translate);
}

void ParticleGroup::SetUVScale(const Vector3& scale)
{
	// 新しいスケール値でUV変換行列を再構築
	materialData_->uvTransform = MakeAffineMatrix(scale, GetUVRotate(), GetUVTranslate());
}

void ParticleGroup::SetUVRotate(const Vector3& rotate)
{
	// 新しい回転値でUV変換行列を再構築
	materialData_->uvTransform = MakeAffineMatrix(GetUVScale(), rotate, GetUVTranslate());
}

void ParticleGroup::UpdateInstanceData(Particle& particle, const Matrix4x4& billboardMatrix, CameraManager* camera)
{
	// 各変換行列を計算
	Matrix4x4 scaleMatrix = MakeScaleMatrix(particle.transform.scale);
	Matrix4x4 rotateMatrix = MakeRotateMatrix(particle.transform.rotate);
	Matrix4x4 translateMatrix = MakeTranslateMatrix(particle.transform.translate);
	// ワールド行列を計算（スケール→回転の順）
	Matrix4x4 worldMatrixInstancing = scaleMatrix * rotateMatrix;
	// ビルボードが有効な場合は適用
	if (isBillboard_)
	{
		worldMatrixInstancing = worldMatrixInstancing * billboardMatrix;
	}
	// 平行移動を適用
	worldMatrixInstancing = worldMatrixInstancing * translateMatrix;
	// WVP行列を計算（ワールド×ビュー×プロジェクション）
	Matrix4x4 wvp = Multiply(worldMatrixInstancing,
							 Multiply(camera->GetActiveCamera()->GetViewMatrix(),
									  camera->GetActiveCamera()->GetProjectionMatrix()));
	// GPU転送用バッファにデータを書き込み
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
	itr->currentTime += TimeManager::GetInstance().GetGameContext().deltaTime;
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
	itr->transform.translate += itr->velocity * TimeManager::GetInstance().GetGameContext().deltaTime; // 1フレーム分の時間を加算
}

void ParticleGroup::UpdateVertexBuffer(const std::vector<VertexData>& vertices)
{
	// 古いリソースを解放
	if (vertexResource)
	{
		vertexResource.Reset();
		vertexData = nullptr;
	}

	// 新しい頂点バッファリソースを作成
	vertexResource = ParticleManager::GetInstance()->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * vertices.size());
	// データをマップして転送
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, vertices.data(), sizeof(VertexData) * vertices.size());
	vertexResource->Unmap(0, nullptr);
	// 頂点バッファービューを再設定
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

void ParticleGroup::MakeCubeVertexData()
{
	// 頂点データを立方体で初期化
	std::vector<VertexData> cubeVertices = ParticleMath::MakeCubeVertexData();
	// 頂点データを更新
	UpdateVertexBuffer(cubeVertices);
}
