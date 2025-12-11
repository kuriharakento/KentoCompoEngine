#include "RibbonRenderer.h"
#include "base/DirectXCommon.h"
#include "base/GraphicsTypes.h"
#include "manager/system/SrvManager.h"
#include "manager/scene/CameraManager.h"
#include "manager/graphics/TextureManager.h"
#include "base/Camera.h"
#include "effects/particle/ParticleManager.h" // Added include
#include "manager/effect/ParticlePipelineManager.h"
#include <cmath>
#include <algorithm>
#include <unordered_set>

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
	std::string path = texturePath.empty() ? "./Resources/uvChecker.png" : texturePath;
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

	// マテリアルリソースの初期化
	materialResource_ = dxCommon->CreateBufferResource(sizeof(RibbonMaterial));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->uvTransform = MakeIdentity4x4();
	materialData_->enableLighting = 0;
	materialData_->useTextureColor = 0;

	// ビュープロジェクションバッファの初期化
	viewProjResource_ = dxCommon->CreateBufferResource(sizeof(Matrix4x4));
	viewProjResource_->Map(0, nullptr, reinterpret_cast<void**>(&viewProjData_));
	*viewProjData_ = MakeIdentity4x4();
}

void RibbonRenderer::Update(const std::vector<Particle>& particles, CameraManager* camera)
{
	// RibbonIDごとにパーティクルをグループ化
	ribbonSegments_.clear();
	vertexCount_ = 0;

	for (const auto& particle : particles)
	{
		if (!particle.IsAlive()) continue;

		RibbonSegment segment;
		segment.position = particle.position;
		segment.tangent = { 0.0f, 0.0f, 1.0f };  // 後で計算
		segment.width = ribbonWidth_;
		segment.color = particle.color;
		segment.normalizedAge = particle.NormalizedAge();  // 0.0～1.0
		segment.timestamp = particle.age;
		segment.isInterpolated = false;

		// ribbonId==0の場合はデフォルトグループ(1)として扱う
		uint32_t groupId = particle.ribbonId > 0 ? particle.ribbonId : 1;
		ribbonSegments_[groupId].push_back(segment);
	}

	// ビュープロジェクション行列を更新
	if (viewProjData_ && camera && camera->GetActiveCamera())
	{
		*viewProjData_ = camera->GetActiveCamera()->GetViewProjectionMatrix();
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
	
	// マテリアル設定を更新
	if (materialData_)
	{
		materialData_->useTextureColor = useTextureColor_ ? 1 : 0;
	}

	// リボン用パイプラインステートの設定
	auto* pm = ParticleManager::GetInstance();
	auto* plm = pm->GetPipelineManager();
	dxCommon->GetCommandList()->SetPipelineState(plm->GetRibbonPipelineState(blendMode_));
	dxCommon->GetCommandList()->SetGraphicsRootSignature(plm->GetRibbonRootSignature());

	// 頂点バッファ設定
	dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);
	dxCommon->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// ViewProjection CBV (Slot 0 - VertexShader)
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, viewProjResource_->GetGPUVirtualAddress());

	// Material CBV (Slot 1 - PixelShader)
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, materialResource_->GetGPUVirtualAddress());

	// Texture SRV (Slot 2 - PixelShader)
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

		// normalizedAgeでソート（古い→新しい = 尾→頭）
		std::sort(segments.begin(), segments.end(),
			[](const RibbonSegment& a, const RibbonSegment& b) {
				return a.normalizedAge > b.normalizedAge;  // normalizedAgeが大きいほど古い
			});

		// セグメント補間（滑らかさ向上）
		if (enableInterpolation_)
		{
			InterpolateSegments(segments);
		}

		GenerateTriangleStrip(segments, cameraPos, allVertices);
	}

	// 頂点バッファにコピー
	vertexCount_ = (std::min)(static_cast<uint32_t>(allVertices.size()), kMaxVertices);
	if (vertexCount_ > 0 && vertexData_)
	{
		memcpy(vertexData_, allVertices.data(), sizeof(RibbonVertex) * vertexCount_);
	}
}

void RibbonRenderer::BuildRibbonMeshFromTrails(CameraManager* camera)
{
	std::vector<RibbonVertex> allVertices;
	allVertices.reserve(kMaxVertices);

	Vector3 cameraPos = camera->GetActiveCamera()->GetTranslate();

	// トレイルからメッシュを構築（セグメントは既にタイムスタンプ順）
	for (auto& [ribbonId, trail] : ribbonTrails_)
	{
		if (trail.segments.size() < 2) continue;

		// トレイルのセグメントからトライアングルストリップを生成
		// セグメントは追加順（新しいものが後ろ）なので、描画時は逆順（古い→新しい）
		GenerateTriangleStrip(trail.segments, cameraPos, allVertices);
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

	// まず全セグメントの接線を計算
	std::vector<Vector3> tangents(segments.size());
	for (size_t i = 0; i < segments.size(); ++i)
	{
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
		
		// 正規化
		float tangentLen = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z);
		if (tangentLen > 0.0001f)
		{
			tangent.x /= tangentLen;
			tangent.y /= tangentLen;
			tangent.z /= tangentLen;
		}
		tangents[i] = tangent;
	}

	// Double-Reflection法: 前セグメントのrightを次に伝播させる
	Vector3 prevRight = { 0.0f, 0.0f, 0.0f };
	
	for (size_t i = 0; i < segments.size(); ++i)
	{
		const auto& seg = segments[i];
		const Vector3& tangent = tangents[i];

		// 年齢ベースのアルファグラデーション（正規化年齢が1に近いほど透明）
		float alphaMultiplier = 1.0f - seg.normalizedAge;

		// 横方向の計算
		Vector3 right;
		
		if (i == 0)
		{
			// 最初のセグメント: 通常の計算で初期化
			if (useBillboard_)
			{
				Vector3 toCamera;
				toCamera.x = cameraPosition.x - seg.position.x;
				toCamera.y = cameraPosition.y - seg.position.y;
				toCamera.z = cameraPosition.z - seg.position.z;
				
				// 外積で横方向を計算
				right.x = tangent.y * toCamera.z - tangent.z * toCamera.y;
				right.y = tangent.z * toCamera.x - tangent.x * toCamera.z;
				right.z = tangent.x * toCamera.y - tangent.y * toCamera.x;
			}
			else
			{
				Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
				right.x = tangent.y * worldUp.z - tangent.z * worldUp.y;
				right.y = tangent.z * worldUp.x - tangent.x * worldUp.z;
				right.z = tangent.x * worldUp.y - tangent.y * worldUp.x;
			}
		}
		else
		{
			// Double-Reflection: 前のrightを現在の接線に対して直交するように調整
			// 1. 前のrightから接線成分を除去（Gram-Schmidt直交化）
			float dot = prevRight.x * tangent.x + prevRight.y * tangent.y + prevRight.z * tangent.z;
			right.x = prevRight.x - dot * tangent.x;
			right.y = prevRight.y - dot * tangent.y;
			right.z = prevRight.z - dot * tangent.z;
			
			// 2. 直交化後のベクトルが小さすぎる場合（接線とほぼ平行だった場合）
			float rightLen = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
			if (rightLen < 0.001f)
			{
				// フォールバック: カメラ方向またはワールドアップから再計算
				if (useBillboard_)
				{
					Vector3 toCamera;
					toCamera.x = cameraPosition.x - seg.position.x;
					toCamera.y = cameraPosition.y - seg.position.y;
					toCamera.z = cameraPosition.z - seg.position.z;
					
					right.x = tangent.y * toCamera.z - tangent.z * toCamera.y;
					right.y = tangent.z * toCamera.x - tangent.x * toCamera.z;
					right.z = tangent.x * toCamera.y - tangent.y * toCamera.x;
				}
				else
				{
					Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
					right.x = tangent.y * worldUp.z - tangent.z * worldUp.y;
					right.y = tangent.z * worldUp.x - tangent.x * worldUp.z;
					right.z = tangent.x * worldUp.y - tangent.y * worldUp.x;
				}
			}
		}

		// 正規化
		float rightLen = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
		if (rightLen > 0.001f)
		{
			right.x /= rightLen;
			right.y /= rightLen;
			right.z /= rightLen;
		}
		
		// 次のセグメントのために保存
		prevRight = right;

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
		// アルファグラデーションを適用
		leftVert.color = seg.color;
		leftVert.color.w *= alphaMultiplier;
		outVertices.push_back(leftVert);

		// 右側の頂点
		RibbonVertex rightVert;
		rightVert.position.x = seg.position.x + right.x * halfWidth;
		rightVert.position.y = seg.position.y + right.y * halfWidth;
		rightVert.position.z = seg.position.z + right.z * halfWidth;
		rightVert.texcoord = { u, 1.0f };
		// アルファグラデーションを適用
		rightVert.color = seg.color;
		rightVert.color.w *= alphaMultiplier;
		outVertices.push_back(rightVert);
	}
}

void RibbonRenderer::InterpolateSegments(std::vector<RibbonSegment>& segments)
{
	if (segments.size() < 2) return;
	
	std::vector<RibbonSegment> interpolated;
	interpolated.reserve(segments.size() * 10);
	
	// Catmull-Rom スプライン補間用ラムダ
	auto catmullRom = [](float p0, float p1, float p2, float p3, float t) -> float {
		float t2 = t * t;
		float t3 = t2 * t;
		return 0.5f * (
			(2.0f * p1) +
			(-p0 + p2) * t +
			(2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
			(-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
		);
	};
	
	for (size_t i = 0; i < segments.size() - 1; ++i)
	{
		// Catmull-Rom用に4点を取得（境界はクランプ）
		size_t i0 = (i > 0) ? i - 1 : 0;
		size_t i1 = i;
		size_t i2 = i + 1;
		size_t i3 = (i + 2 < segments.size()) ? i + 2 : segments.size() - 1;
		
		const auto& p0 = segments[i0];
		const auto& p1 = segments[i1];
		const auto& p2 = segments[i2];
		const auto& p3 = segments[i3];
		
		// 現在のセグメントを追加
		interpolated.push_back(p1);
		
		// 距離を計算
		float dx = p2.position.x - p1.position.x;
		float dy = p2.position.y - p1.position.y;
		float dz = p2.position.z - p1.position.z;
		float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
		
		// 距離に基づいて補間点の数を計算（最侎3つ）
		int numInterpolations = (std::max)(3, static_cast<int>(std::ceil(distance / maxSegmentDistance_)));
		
		// Catmull-Rom補間点を追加
		for (int j = 1; j <= numInterpolations; ++j)
		{
			float t = static_cast<float>(j) / static_cast<float>(numInterpolations + 1);
			
			RibbonSegment interp;
			// 位置のCatmull-Rom補間（滑らかな曲線）
			interp.position.x = catmullRom(p0.position.x, p1.position.x, p2.position.x, p3.position.x, t);
			interp.position.y = catmullRom(p0.position.y, p1.position.y, p2.position.y, p3.position.y, t);
			interp.position.z = catmullRom(p0.position.z, p1.position.z, p2.position.z, p3.position.z, t);
			// 接線の線形補間
			interp.tangent.x = p1.tangent.x + (p2.tangent.x - p1.tangent.x) * t;
			interp.tangent.y = p1.tangent.y + (p2.tangent.y - p1.tangent.y) * t;
			interp.tangent.z = p1.tangent.z + (p2.tangent.z - p1.tangent.z) * t;
			// 幅の線形補間
			interp.width = p1.width + (p2.width - p1.width) * t;
			// 色の線形補間
			interp.color.x = p1.color.x + (p2.color.x - p1.color.x) * t;
			interp.color.y = p1.color.y + (p2.color.y - p1.color.y) * t;
			interp.color.z = p1.color.z + (p2.color.z - p1.color.z) * t;
			interp.color.w = p1.color.w + (p2.color.w - p1.color.w) * t;
			// normalizedAgeの線形補間
			interp.normalizedAge = p1.normalizedAge + (p2.normalizedAge - p1.normalizedAge) * t;
			interp.timestamp = p1.timestamp + (p2.timestamp - p1.timestamp) * t;
			interp.isInterpolated = true;  // 補間セグメントをマーク
			
			interpolated.push_back(interp);
		}
	}
	
	// 最後のセグメントを追加
	interpolated.push_back(segments.back());
	
	// 結果で置き換え
	segments = std::move(interpolated);
}

// 先頭セグメントとの補間（新規追加時のみ）
std::vector<RibbonRenderer::RibbonSegment> RibbonRenderer::InterpolateWithHead(
	const RibbonSegment& head, const RibbonSegment& newSegment)
{
	std::vector<RibbonSegment> result;
	
	// 距離を計算
	float dx = newSegment.position.x - head.position.x;
	float dy = newSegment.position.y - head.position.y;
	float dz = newSegment.position.z - head.position.z;
	float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
	
	// 距離が短すぎる場合は補間不要
	if (distance < maxSegmentDistance_) return result;
	
	// 距離に基づいて補間点の数を計算
	int numInterpolations = static_cast<int>(std::ceil(distance / maxSegmentDistance_)) - 1;
	
	for (int j = 1; j <= numInterpolations; ++j)
	{
		float t = static_cast<float>(j) / static_cast<float>(numInterpolations + 1);
		
		RibbonSegment interp;
		// 位置の線形補間
		interp.position.x = head.position.x + (newSegment.position.x - head.position.x) * t;
		interp.position.y = head.position.y + (newSegment.position.y - head.position.y) * t;
		interp.position.z = head.position.z + (newSegment.position.z - head.position.z) * t;
		// 接線の線形補間
		interp.tangent.x = head.tangent.x + (newSegment.tangent.x - head.tangent.x) * t;
		interp.tangent.y = head.tangent.y + (newSegment.tangent.y - head.tangent.y) * t;
		interp.tangent.z = head.tangent.z + (newSegment.tangent.z - head.tangent.z) * t;
		// 幅の線形補間
		interp.width = head.width + (newSegment.width - head.width) * t;
		// 色の線形補間
		interp.color.x = head.color.x + (newSegment.color.x - head.color.x) * t;
		interp.color.y = head.color.y + (newSegment.color.y - head.color.y) * t;
		interp.color.z = head.color.z + (newSegment.color.z - head.color.z) * t;
		interp.color.w = head.color.w + (newSegment.color.w - head.color.w) * t;
		// normalizedAgeの線形補間
		interp.normalizedAge = head.normalizedAge + (newSegment.normalizedAge - head.normalizedAge) * t;
		interp.timestamp = head.timestamp + (newSegment.timestamp - head.timestamp) * t;
		interp.isInterpolated = true;
		
		result.push_back(interp);
	}
	
	return result;
}

