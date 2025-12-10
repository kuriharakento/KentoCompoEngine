#include "RibbonRenderer.h"
#include "base/DirectXCommon.h"
#include "manager/system/SrvManager.h"
#include "manager/scene/CameraManager.h"
#include "manager/graphics/TextureManager.h"
#include "base/Camera.h"
#include "effects/particle/ParticleManager.h" // Added include
#include "manager/effect/ParticlePipelineManager.h"
#include <cmath>
#include <algorithm>

RibbonRenderer::~RibbonRenderer()
{
	if (vertexResource_)
	{
		vertexResource_->Unmap(0, nullptr);
	}
}

void RibbonRenderer::Initialize(const std::string& texturePath)
{
	// テクスチャ読み込み
	std::string path = texturePath.empty() ? "white1x1.png" : texturePath;
	TextureManager::GetInstance()->LoadTexture(path);
	textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(path);

	auto* pm = ParticleManager::GetInstance();
	InitializeBuffers(pm->GetDxCommon());
}

void RibbonRenderer::InitializeBuffers(DirectXCommon* dxCommon)
{
	if (vertexResource_) return; // 既に初期化済み

	// 頂点バッファ作成
	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeof(RibbonVertex) * kMaxVertices;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	dxCommon->GetDevice()->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertexResource_)
	);

	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(RibbonVertex) * kMaxVertices;
	vertexBufferView_.StrideInBytes = sizeof(RibbonVertex);
}

void RibbonRenderer::Update(const std::vector<Particle>& particles, CameraManager* camera)
{
	// RibbonIDごとにパーティクルをグループ化
	ribbonSegments_.clear();
	vertexCount_ = 0; // Reset vertex count when clearing

	for (const auto& particle : particles)
	{
		if (!particle.IsAlive()) continue;
		if (particle.ribbonId == 0) continue; // RibbonID 0は無効

		RibbonSegment segment;
		segment.position = particle.position;
		segment.width = particle.ribbonWidth;
		segment.color = particle.color;
		segment.age = particle.age;

		ribbonSegments_[particle.ribbonId].push_back(segment);
	}

	// リボンメッシュを構築
	BuildRibbonMesh(camera);
}

void RibbonRenderer::SetTexture(const std::string& texturePath)
{
	TextureManager::GetInstance()->LoadTexture(texturePath);
	textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(texturePath);
}

void RibbonRenderer::Draw(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	// バッファが未初期化なら初期化
	InitializeBuffers(dxCommon);

	if (vertexCount_ == 0) return;

	// パイプラインステートの設定（ブレンドモード反映）
	auto* pm = ParticleManager::GetInstance();
	auto* plm = pm->GetPipelineManager();
	dxCommon->GetCommandList()->SetPipelineState(plm->GetPipelineState(blendMode_));

	// 頂点バッファ設定
	dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);
	dxCommon->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// テクスチャ設定
	srvManager->SetGraphicsRootDescriptorTable(2, textureIndex_);

	// 描画
	dxCommon->GetCommandList()->DrawInstanced(vertexCount_, 1, 0, 0);
}

void RibbonRenderer::BuildRibbonMesh(CameraManager* camera)
{
	std::vector<RibbonVertex> allVertices;
	allVertices.reserve(kMaxVertices);

	Vector3 cameraPos = camera->GetActiveCamera()->GetTranslate();

	for (auto& [ribbonId, segments] : ribbonSegments_)
	{
		if (segments.size() < 2) continue;

		// 古い順にソート（ageが大きいほど古い）
		std::sort(segments.begin(), segments.end(),
			[](const RibbonSegment& a, const RibbonSegment& b) {
				return a.age > b.age;
			});

		GenerateTriangleStrip(segments, cameraPos, allVertices);
	}

	// 頂点バッファにコピー
	vertexCount_ = (std::min)(static_cast<uint32_t>(allVertices.size()), kMaxVertices);
	if (vertexCount_ > 0 && vertexData_)
	{
		memcpy(vertexData_, allVertices.data(), sizeof(RibbonVertex) * vertexCount_);
	}
}

void RibbonRenderer::GenerateTriangleStrip(
	const std::vector<RibbonSegment>& segments,
	const Vector3& cameraPosition,
	std::vector<RibbonVertex>& outVertices)
{
	if (segments.size() < 2) return;

	float totalLength = 0.0f;
	std::vector<float> segmentLengths;
	segmentLengths.reserve(segments.size());
	segmentLengths.push_back(0.0f);

	// 各セグメントの長さを計算
	for (size_t i = 1; i < segments.size(); ++i)
	{
		Vector3 diff;
		diff.x = segments[i].position.x - segments[i - 1].position.x;
		diff.y = segments[i].position.y - segments[i - 1].position.y;
		diff.z = segments[i].position.z - segments[i - 1].position.z;

		float length = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
		totalLength += length;
		segmentLengths.push_back(totalLength);
	}

	// トライアングルストリップを生成
	for (size_t i = 0; i < segments.size(); ++i)
	{
		const auto& seg = segments[i];

		// 接線方向を計算
		Vector3 tangent;
		if (i == 0)
		{
			tangent.x = segments[1].position.x - segments[0].position.x;
			tangent.y = segments[1].position.y - segments[0].position.y;
			tangent.z = segments[1].position.z - segments[0].position.z;
		}
		else if (i == segments.size() - 1)
		{
			tangent.x = segments[i].position.x - segments[i - 1].position.x;
			tangent.y = segments[i].position.y - segments[i - 1].position.y;
			tangent.z = segments[i].position.z - segments[i - 1].position.z;
		}
		else
		{
			tangent.x = segments[i + 1].position.x - segments[i - 1].position.x;
			tangent.y = segments[i + 1].position.y - segments[i - 1].position.y;
			tangent.z = segments[i + 1].position.z - segments[i - 1].position.z;
		}

		// カメラへの方向
		Vector3 toCamera;
		toCamera.x = cameraPosition.x - seg.position.x;
		toCamera.y = cameraPosition.y - seg.position.y;
		toCamera.z = cameraPosition.z - seg.position.z;

		// 外積で横方向を計算
		Vector3 right;
		right.x = tangent.y * toCamera.z - tangent.z * toCamera.y;
		right.y = tangent.z * toCamera.x - tangent.x * toCamera.z;
		right.z = tangent.x * toCamera.y - tangent.y * toCamera.x;

		// 正規化
		float rightLen = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
		if (rightLen > 0.001f)
		{
			right.x /= rightLen;
			right.y /= rightLen;
			right.z /= rightLen;
		}

		float halfWidth = seg.width * 0.5f;

		// UV計算
		float u = 0.0f;
		if (textureMode_ == RibbonTextureMode::Stretch)
		{
			u = (totalLength > 0.0f) ? segmentLengths[i] / totalLength : 0.0f;
		}
		else // Tile
		{
			u = segmentLengths[i] * tileScale_;
		}

		// 左側の頂点
		RibbonVertex leftVert;
		leftVert.position.x = seg.position.x - right.x * halfWidth;
		leftVert.position.y = seg.position.y - right.y * halfWidth;
		leftVert.position.z = seg.position.z - right.z * halfWidth;
		leftVert.texcoord = { u, 0.0f };
		leftVert.color = seg.color;
		outVertices.push_back(leftVert);

		// 右側の頂点
		RibbonVertex rightVert;
		rightVert.position.x = seg.position.x + right.x * halfWidth;
		rightVert.position.y = seg.position.y + right.y * halfWidth;
		rightVert.position.z = seg.position.z + right.z * halfWidth;
		rightVert.texcoord = { u, 1.0f };
		rightVert.color = seg.color;
		outVertices.push_back(rightVert);
	}
}
