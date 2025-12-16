#include "TrailRenderer.h"
#include "effects/particle/ParticleManager.h"
#include "manager/effect/ParticlePipelineManager.h"
#include "manager/scene/CameraManager.h"
#include "manager/system/SrvManager.h"
#include "manager/graphics/TextureManager.h"
#include "base/DirectXCommon.h"
#include "time/TimeManager.h"
#include <cmath>
#include <algorithm>

TrailRenderer::~TrailRenderer()
{
	if (vertexResource_)
	{
		vertexResource_->Unmap(0, nullptr);
		vertexResource_.Reset();
	}
	if (materialResource_)
	{
		materialResource_->Unmap(0, nullptr);
		materialResource_.Reset();
	}
	if (viewProjResource_)
	{
		viewProjResource_->Unmap(0, nullptr);
		viewProjResource_.Reset();
	}
}

void TrailRenderer::Initialize(const std::string& texturePath)
{
	std::string path = texturePath.empty() ? "./Resources/uvChecker.png" : texturePath;
	texturePath_ = path;
	TextureManager::GetInstance()->LoadTexture(path);
	textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(path);

	auto* pm = ParticleManager::GetInstance();
	InitializeBuffers(pm->GetDxCommon(), pm->GetSrvManager());
}

void TrailRenderer::InitializeBuffers(DirectXCommon* dxCommon, SrvManager* /*srvManager*/)
{
	// マテリアルバッファ
	materialResource_ = dxCommon->CreateBufferResource(sizeof(TrailMaterial));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->uvTransform = MakeIdentity4x4();
	materialData_->enableLighting = 0;
	materialData_->useTextureColor = 1;

	// 頂点バッファ
	vertexResource_ = dxCommon->CreateBufferResource(sizeof(TrailVertex) * kMaxVertices);
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.StrideInBytes = sizeof(TrailVertex);
	vertexBufferView_.SizeInBytes = sizeof(TrailVertex) * kMaxVertices;

	// ビュープロジェクション行列バッファ
	viewProjResource_ = dxCommon->CreateBufferResource(sizeof(Matrix4x4));
	viewProjResource_->Map(0, nullptr, reinterpret_cast<void**>(&viewProjData_));
	*viewProjData_ = MakeIdentity4x4();
}

void TrailRenderer::SetTexture(const std::string& texturePath)
{
	texturePath_ = texturePath;
	TextureManager::GetInstance()->LoadTexture(texturePath);
	textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(texturePath);
}

void TrailRenderer::Update(const std::vector<Particle>& particles, CameraManager* camera)
{
	// デルタタイム取得
	float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;
	lastDeltaTime_ = deltaTime;
	currentTime_ += deltaTime;

	// 新方式: パーティクルを接続してリボンメッシュを構築
	BuildRibbonFromParticles(particles, camera);
}

void TrailRenderer::UpdateTrails(const std::vector<Particle>& particles, float deltaTime)
{
	// 現在アクティブなパーティクルIDを記録
	std::unordered_map<uint32_t, bool> activeParticles;
	for (const auto& particle : particles)
	{
		if (particle.IsAlive())
		{
			activeParticles[particle.id] = true;
		}
	}

	// 既存トレイルの更新
	for (auto it = trails_.begin(); it != trails_.end();)
	{
		bool isActive = activeParticles.find(it->first) != activeParticles.end();
		it->second.isActive = isActive;

		// セグメントの経過時間を更新
		for (auto& segment : it->second.segments)
		{
			segment.age += deltaTime;
		}

		// 寿命を超えたセグメントを削除
		while (!it->second.segments.empty() &&
		       it->second.segments.back().age > it->second.segments.back().lifetime)
		{
			it->second.segments.pop_back();
		}

		// 非アクティブでセグメントが空なら削除
		if (!isActive && it->second.segments.empty())
		{
			it = trails_.erase(it);
		}
		else
		{
			++it;
		}
	}

	// パーティクルごとにトレイルを更新
	for (const auto& particle : particles)
	{
		if (!particle.IsAlive()) continue;

		uint32_t particleId = particle.id;
		auto& trail = trails_[particleId];

		// パーティクルが動いているかチェック（速度が一定以上）
		float velocityMagnitude = std::sqrt(
			particle.velocity.x * particle.velocity.x +
			particle.velocity.y * particle.velocity.y +
			particle.velocity.z * particle.velocity.z
		);
		const float kMinVelocity = 0.01f;  // 最小速度閾値
		if (velocityMagnitude < kMinVelocity && !trail.segments.empty())
		{
			// 動いていなければ新しいセグメントは追加しない
			continue;
		}

		// 距離チェック（まず距離を確認）
		if (!trail.segments.empty())
		{
			const auto& lastSegment = trail.segments.front();
			float dx = particle.position.x - lastSegment.position.x;
			float dy = particle.position.y - lastSegment.position.y;
			float dz = particle.position.z - lastSegment.position.z;
			float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

			if (distance < minSegmentDistance_)
			{
				continue;
			}
		}

		// 記録間隔チェック
		if (!trail.segments.empty())
		{
			float timeSinceLastRecord = currentTime_ - trail.lastRecordTime;
			if (timeSinceLastRecord < recordInterval_)
			{
				continue;
			}
		}

		// 新しいセグメントを作成
		TrailSegment newSegment;
		newSegment.position = particle.position;
		newSegment.width = trailWidth_ * particle.scale.x;
		newSegment.color = particle.color;
		newSegment.age = 0.0f;
		newSegment.lifetime = trailLifetime_;

		// 方向を計算
		if (!trail.segments.empty())
		{
			const auto& prev = trail.segments.front();
			newSegment.direction.x = newSegment.position.x - prev.position.x;
			newSegment.direction.y = newSegment.position.y - prev.position.y;
			newSegment.direction.z = newSegment.position.z - prev.position.z;
			
			// 正規化
			float length = std::sqrt(
				newSegment.direction.x * newSegment.direction.x +
				newSegment.direction.y * newSegment.direction.y +
				newSegment.direction.z * newSegment.direction.z
			);
			if (length > 0.0001f)
			{
				newSegment.direction.x /= length;
				newSegment.direction.y /= length;
				newSegment.direction.z /= length;
			}

			// 先頭に追加
			trail.segments.push_front(newSegment);
		}
		else
		{
			// 最初のセグメント - 速度から過去位置を推測して2つのセグメントを同時に追加
			float vlen = std::sqrt(
				particle.velocity.x * particle.velocity.x +
				particle.velocity.y * particle.velocity.y +
				particle.velocity.z * particle.velocity.z
			);

			Vector3 direction;
			if (vlen > 0.0001f)
			{
				direction.x = particle.velocity.x / vlen;
				direction.y = particle.velocity.y / vlen;
				direction.z = particle.velocity.z / vlen;
			}
			else
			{
				// 速度がない場合はデフォルト方向（上向き = 重力の反対）を使用
				direction = { 0.0f, 1.0f, 0.0f };
				vlen = 1.0f;  // デフォルト距離用
			}

			newSegment.direction = direction;

			// 過去の位置を推測（進行方向の逆に一定距離）
			float backDistance = (std::max)(minSegmentDistance_ * 2.0f, vlen * recordInterval_ * 2.0f);
			TrailSegment pastSegment;
			pastSegment.position.x = particle.position.x - direction.x * backDistance;
			pastSegment.position.y = particle.position.y - direction.y * backDistance;
			pastSegment.position.z = particle.position.z - direction.z * backDistance;
			pastSegment.direction = direction;
			pastSegment.width = newSegment.width;
			pastSegment.color = newSegment.color;
			pastSegment.age = recordInterval_;  // 少し古い
			pastSegment.lifetime = trailLifetime_;

			// 現在位置を先頭に、過去位置を末尾に追加
			trail.segments.push_front(newSegment);
			trail.segments.push_back(pastSegment);
		}

		trail.lastRecordTime = currentTime_;
		trail.isActive = true;

		// 最大セグメント数を制限
		while (trail.segments.size() > 100)
		{
			trail.segments.pop_back();
		}
	}
}

void TrailRenderer::BuildTrailMesh(CameraManager* camera)
{
	if (!vertexData_) return;

	Vector3 cameraPosition = camera->GetActiveCamera()->GetTranslate();

	std::vector<TrailVertex> allVertices;
	allVertices.reserve(kMaxVertices);

	for (const auto& [particleId, trail] : trails_)
	{
		if (trail.segments.size() < 2) continue;

		// 前のトレイルがある場合、degenerate triangleを追加して分離
		size_t prevSize = allVertices.size();

		GenerateTriangleStrip(trail.segments, cameraPosition, allVertices);

		// degenerate triangle: 2つのトレイル間に重複頂点を挿入
		if (prevSize > 0 && allVertices.size() > prevSize)
		{
			// 前の最後の頂点と新しい最初の頂点を追加
			TrailVertex lastOfPrev = allVertices[prevSize - 1];
			TrailVertex firstOfNew = allVertices[prevSize];
			allVertices.insert(allVertices.begin() + prevSize, firstOfNew);
			allVertices.insert(allVertices.begin() + prevSize, lastOfPrev);
		}

		if (allVertices.size() >= kMaxVertices - 100)
		{
			break;  // 頂点数上限
		}
	}

	// 頂点データをコピー
	vertexCount_ = static_cast<uint32_t>((std::min)(allVertices.size(), static_cast<size_t>(kMaxVertices)));
	if (vertexCount_ > 0)
	{
		std::memcpy(vertexData_, allVertices.data(), sizeof(TrailVertex) * vertexCount_);
	}

	// ビュープロジェクション行列を更新
	if (viewProjData_)
	{
		*viewProjData_ = Multiply(
			camera->GetActiveCamera()->GetViewMatrix(),
			camera->GetActiveCamera()->GetProjectionMatrix()
		);
	}
}

void TrailRenderer::BuildRibbonFromParticles(const std::vector<Particle>& particles, CameraManager* camera)
{
	if (!vertexData_) return;

	// RibbonIdごとにパーティクルをグループ化
	std::unordered_map<uint32_t, std::vector<const Particle*>> ribbonGroups;
	for (const auto& particle : particles)
	{
		if (particle.IsAlive())
		{
			ribbonGroups[particle.ribbonId].push_back(&particle);
		}
	}

	Vector3 cameraPosition = camera->GetActiveCamera()->GetTranslate();
	std::vector<TrailVertex> allVertices;
	allVertices.reserve(kMaxVertices);

	// 各リボングループを処理
	for (auto& [ribbonId, group] : ribbonGroups)
	{
		if (group.size() < 2) continue;

		// 年齢順にソート（新しい順 = age小さい順）
		std::sort(group.begin(), group.end(),
			[](const Particle* a, const Particle* b) { return a->age < b->age; });

		// 前のリボンがある場合、degenerate triangleを追加して分離
		size_t prevSize = allVertices.size();

		// このグループのリボン頂点を生成
		GenerateRibbonVertices(group, cameraPosition, allVertices);

		// degenerate triangle: 2つのリボン間に重複頂点を挿入
		if (prevSize > 0 && allVertices.size() > prevSize)
		{
			TrailVertex lastOfPrev = allVertices[prevSize - 1];
			TrailVertex firstOfNew = allVertices[prevSize];
			allVertices.insert(allVertices.begin() + prevSize, firstOfNew);
			allVertices.insert(allVertices.begin() + prevSize, lastOfPrev);
		}

		if (allVertices.size() >= kMaxVertices - 100)
		{
			break;  // 頂点数上限
		}
	}

	// 頂点データをコピー
	vertexCount_ = static_cast<uint32_t>((std::min)(allVertices.size(), static_cast<size_t>(kMaxVertices)));
	if (vertexCount_ > 0)
	{
		std::memcpy(vertexData_, allVertices.data(), sizeof(TrailVertex) * vertexCount_);
	}

	// ビュープロジェクション行列を更新
	if (viewProjData_)
	{
		*viewProjData_ = Multiply(
			camera->GetActiveCamera()->GetViewMatrix(),
			camera->GetActiveCamera()->GetProjectionMatrix()
		);
	}
}

void TrailRenderer::GenerateRibbonVertices(
	const std::vector<const Particle*>& group,
	const Vector3& cameraPosition,
	std::vector<TrailVertex>& outVertices)
{
	if (group.size() < 2) return;

	// パーティクル間の距離を計算
	float totalLength = 0.0f;
	std::vector<float> cumulativeLengths;
	cumulativeLengths.reserve(group.size());
	cumulativeLengths.push_back(0.0f);

	for (size_t i = 1; i < group.size(); ++i)
	{
		const auto* curr = group[i];
		const auto* prev = group[i - 1];
		float dx = curr->position.x - prev->position.x;
		float dy = curr->position.y - prev->position.y;
		float dz = curr->position.z - prev->position.z;
		totalLength += std::sqrt(dx * dx + dy * dy + dz * dz);
		cumulativeLengths.push_back(totalLength);
	}

	// 各パーティクルに対して左右の頂点を生成
	for (size_t i = 0; i < group.size(); ++i)
	{
		const auto* particle = group[i];

		// 年齢比率（0=新しい, 1=古い）
		float ageRatio = (particle->lifetime > 0.0f) ? particle->age / particle->lifetime : 0.0f;
		ageRatio = (std::clamp)(ageRatio, 0.0f, 1.0f);

		// 幅の計算
		float width = trailWidth_ * particle->scale.x;
		if (widthFade_)
		{
			width *= (1.0f - ageRatio);
		}

		// 色とアルファの計算
		Vector4 color = particle->color;
		if (alphaFade_)
		{
			color.w *= (1.0f - ageRatio);
		}

		// 接線を計算（隣接パーティクル間）
		Vector3 tangent = { 0.0f, 0.0f, 1.0f };
		if (i < group.size() - 1)
		{
			const auto* next = group[i + 1];
			tangent.x = next->position.x - particle->position.x;
			tangent.y = next->position.y - particle->position.y;
			tangent.z = next->position.z - particle->position.z;
		}
		else if (i > 0)
		{
			const auto* prev = group[i - 1];
			tangent.x = particle->position.x - prev->position.x;
			tangent.y = particle->position.y - prev->position.y;
			tangent.z = particle->position.z - prev->position.z;
		}

		// 正規化
		float tangentLen = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z);
		if (tangentLen > 0.0001f)
		{
			tangent.x /= tangentLen;
			tangent.y /= tangentLen;
			tangent.z /= tangentLen;
		}

		// Right軸を計算（ビルボード）
		Vector3 toCamera;
		toCamera.x = cameraPosition.x - particle->position.x;
		toCamera.y = cameraPosition.y - particle->position.y;
		toCamera.z = cameraPosition.z - particle->position.z;

		Vector3 right;
		if (useBillboard_)
		{
			// クロス積: tangent × toCamera
			right.x = tangent.y * toCamera.z - tangent.z * toCamera.y;
			right.y = tangent.z * toCamera.x - tangent.x * toCamera.z;
			right.z = tangent.x * toCamera.y - tangent.y * toCamera.x;
		}
		else
		{
			// 固定軸（Y軸を基準）
			right.x = tangent.z;
			right.y = 0.0f;
			right.z = -tangent.x;
		}

		// 正規化とフォールバック
		float rightLen = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
		if (rightLen > 0.0001f)
		{
			right.x /= rightLen;
			right.y /= rightLen;
			right.z /= rightLen;
		}
		else
		{
			// フォールバック: 垂直移動の場合
			float tangentY = std::abs(tangent.y);
			if (tangentY > 0.9f)
			{
				right = { 1.0f, 0.0f, 0.0f };
			}
			else
			{
				right.x = tangent.z;
				right.y = 0.0f;
				right.z = -tangent.x;
				rightLen = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
				if (rightLen > 0.0001f)
				{
					right.x /= rightLen;
					right.y /= rightLen;
					right.z /= rightLen;
				}
				else
				{
					right = { 1.0f, 0.0f, 0.0f };
				}
			}
		}

		// 左右の頂点を計算
		float halfWidth = width * 0.5f;
		Vector3 leftPos = {
			particle->position.x - right.x * halfWidth,
			particle->position.y - right.y * halfWidth,
			particle->position.z - right.z * halfWidth
		};
		Vector3 rightPos = {
			particle->position.x + right.x * halfWidth,
			particle->position.y + right.y * halfWidth,
			particle->position.z + right.z * halfWidth
		};

		// UV座標
		float v = 0.0f;
		if (textureMode_ == RibbonTextureMode::Stretch)
		{
			v = (totalLength > 0.0f) ? cumulativeLengths[i] / totalLength : 0.0f;
		}
		else
		{
			v = cumulativeLengths[i] * tileScale_;
		}

		// 頂点を追加
		TrailVertex leftVertex;
		leftVertex.position = leftPos;
		leftVertex.texcoord = { 0.0f, v };
		leftVertex.color = color;

		TrailVertex rightVertex;
		rightVertex.position = rightPos;
		rightVertex.texcoord = { 1.0f, v };
		rightVertex.color = color;

		outVertices.push_back(leftVertex);
		outVertices.push_back(rightVertex);
	}
}

void TrailRenderer::GenerateTriangleStrip(
	const std::deque<TrailSegment>& segments,
	const Vector3& cameraPosition,
	std::vector<TrailVertex>& outVertices)
{
	if (segments.size() < 2) return;

	float totalLength = 0.0f;
	std::vector<float> cumulativeLengths;
	cumulativeLengths.reserve(segments.size());
	cumulativeLengths.push_back(0.0f);

	for (size_t i = 1; i < segments.size(); ++i)
	{
		float dx = segments[i].position.x - segments[i - 1].position.x;
		float dy = segments[i].position.y - segments[i - 1].position.y;
		float dz = segments[i].position.z - segments[i - 1].position.z;
		float segLength = std::sqrt(dx * dx + dy * dy + dz * dz);
		totalLength += segLength;
		cumulativeLengths.push_back(totalLength);
	}

	for (size_t i = 0; i < segments.size(); ++i)
	{
		const auto& segment = segments[i];

		// 年齢に基づくフェード係数
		float ageRatio = segment.lifetime > 0.0f ? segment.age / segment.lifetime : 1.0f;
		ageRatio = (std::clamp)(ageRatio, 0.0f, 1.0f);

		// 幅の計算
		float width = segment.width;
		if (widthFade_)
		{
			width *= (1.0f - ageRatio);
		}

		// 色とアルファの計算
		Vector4 color = segment.color;
		if (alphaFade_)
		{
			color.w *= (1.0f - ageRatio);
		}

		// 接線を計算
		Vector3 tangent = segment.direction;
		if (i < segments.size() - 1)
		{
			tangent.x = segments[i + 1].position.x - segment.position.x;
			tangent.y = segments[i + 1].position.y - segment.position.y;
			tangent.z = segments[i + 1].position.z - segment.position.z;
		}
		else if (i > 0)
		{
			tangent.x = segment.position.x - segments[i - 1].position.x;
			tangent.y = segment.position.y - segments[i - 1].position.y;
			tangent.z = segment.position.z - segments[i - 1].position.z;
		}

		// 法線を計算（カメラ方向とのクロス積）
		Vector3 toCamera;
		toCamera.x = cameraPosition.x - segment.position.x;
		toCamera.y = cameraPosition.y - segment.position.y;
		toCamera.z = cameraPosition.z - segment.position.z;

		Vector3 right;
		if (useBillboard_)
		{
			// クロス積: tangent × toCamera
			right.x = tangent.y * toCamera.z - tangent.z * toCamera.y;
			right.y = tangent.z * toCamera.x - tangent.x * toCamera.z;
			right.z = tangent.x * toCamera.y - tangent.y * toCamera.x;
		}
		else
		{
			// Y軸を基準にしたクロス積
			right.x = tangent.z;
			right.y = 0.0f;
			right.z = -tangent.x;
		}

		// 正規化
		float rightLen = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
		if (rightLen > 0.0001f)
		{
			right.x /= rightLen;
			right.y /= rightLen;
			right.z /= rightLen;
		}
		else
		{
			// フォールバック: tangentとtoCameraが平行な場合
			// tangentがY軸と平行かチェック
			float tangentY = std::abs(tangent.y) / std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z + 0.0001f);
			
			if (tangentY > 0.9f)
			{
				// 垂直移動の場合はworldRight(1,0,0)とのクロス積を使用
				// tangent × (1,0,0) = (0, tangent.z, -tangent.y)
				right.x = 0.0f;
				right.y = tangent.z;
				right.z = -tangent.y;
			}
			else
			{
				// それ以外はworldUp(0,1,0)とのクロス積を使用
				// tangent × (0,1,0) = (tangent.z, 0, -tangent.x)
				right.x = tangent.z;
				right.y = 0.0f;
				right.z = -tangent.x;
			}
			
			rightLen = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
			if (rightLen > 0.0001f)
			{
				right.x /= rightLen;
				right.y /= rightLen;
				right.z /= rightLen;
			}
			else
			{
				// 最終フォールバック: 固定のRight軸
				right = { 1.0f, 0.0f, 0.0f };
			}
		}

		// 左右の頂点を計算
		float halfWidth = width * 0.5f;
		Vector3 leftPos = {
			segment.position.x - right.x * halfWidth,
			segment.position.y - right.y * halfWidth,
			segment.position.z - right.z * halfWidth
		};
		Vector3 rightPos = {
			segment.position.x + right.x * halfWidth,
			segment.position.y + right.y * halfWidth,
			segment.position.z + right.z * halfWidth
		};

		// UV座標
		float v = 0.0f;
		if (textureMode_ == RibbonTextureMode::Stretch)
		{
			v = (totalLength > 0.0f) ? cumulativeLengths[i] / totalLength : 0.0f;
		}
		else // Tile
		{
			v = cumulativeLengths[i] * tileScale_;
		}

		// 頂点を追加
		TrailVertex leftVertex;
		leftVertex.position = leftPos;
		leftVertex.texcoord = { 0.0f, v };
		leftVertex.color = color;

		TrailVertex rightVertex;
		rightVertex.position = rightPos;
		rightVertex.texcoord = { 1.0f, v };
		rightVertex.color = color;

		outVertices.push_back(leftVertex);
		outVertices.push_back(rightVertex);
	}
}

void TrailRenderer::InterpolateSegments(
	const TrailSegment& from,
	const TrailSegment& to,
	std::vector<TrailSegment>& outSegments)
{
	float dx = to.position.x - from.position.x;
	float dy = to.position.y - from.position.y;
	float dz = to.position.z - from.position.z;
	float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

	if (distance <= minSegmentDistance_)
	{
		return;
	}

	int numSteps = static_cast<int>(std::ceil(distance / minSegmentDistance_));
	for (int i = 1; i < numSteps; ++i)
	{
		float t = static_cast<float>(i) / static_cast<float>(numSteps);

		TrailSegment interp;
		interp.position.x = from.position.x + dx * t;
		interp.position.y = from.position.y + dy * t;
		interp.position.z = from.position.z + dz * t;
		interp.direction = from.direction;
		interp.width = from.width + (to.width - from.width) * t;
		interp.color.x = from.color.x + (to.color.x - from.color.x) * t;
		interp.color.y = from.color.y + (to.color.y - from.color.y) * t;
		interp.color.z = from.color.z + (to.color.z - from.color.z) * t;
		interp.color.w = from.color.w + (to.color.w - from.color.w) * t;
		interp.age = from.age + (to.age - from.age) * t;
		interp.lifetime = from.lifetime;

		outSegments.push_back(interp);
	}
}

void TrailRenderer::Draw(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	if (vertexCount_ < 4) return;

	auto commandList = dxCommon->GetCommandList();
	auto pipelineManager = ParticleManager::GetInstance()->GetPipelineManager();

	// リボン用パイプラインを使用
	commandList->SetPipelineState(pipelineManager->GetRibbonPipelineState(blendMode_));
	commandList->SetGraphicsRootSignature(pipelineManager->GetRibbonRootSignature());

	// プリミティブトポロジ設定
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// 頂点バッファ設定
	D3D12_VERTEX_BUFFER_VIEW vbView = vertexBufferView_;
	vbView.SizeInBytes = sizeof(TrailVertex) * vertexCount_;
	commandList->IASetVertexBuffers(0, 1, &vbView);

	// ビュープロジェクション行列 (Slot 0)
	commandList->SetGraphicsRootConstantBufferView(0, viewProjResource_->GetGPUVirtualAddress());

	// マテリアル (Slot 1)
	commandList->SetGraphicsRootConstantBufferView(1, materialResource_->GetGPUVirtualAddress());

	// テクスチャ (Slot 2)
	commandList->SetGraphicsRootDescriptorTable(2, srvManager->GetGPUDescriptorHandle(textureIndex_));

	// 描画
	commandList->DrawInstanced(vertexCount_, 1, 0, 0);
}
