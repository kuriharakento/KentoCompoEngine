#include "Sprite.h"

#include <algorithm>
#include <cmath>

#include "SpriteCommon.h"
#include "graphics/FrameConstantAllocator.h"
#include "manager/graphics/TextureManager.h"

namespace KCE
{
// 四角のスプライトの頂点数
constexpr int kVertexCount = 4;
// 四角のスプライトのインデックス数
constexpr int kIndexCount = 6;
// ラジアルワイプの最大の頂点数（中心 + 開始の辺 + 角4つ + 終わりの辺）
constexpr int kMaxVertexCount = 8;
// ラジアルワイプの最大のインデックス数（三角形 6 つ）
constexpr int kMaxIndexCount = 18;
constexpr float kTwoPi = 6.28318530718f;
// Z座標のデフォルト値
constexpr float kDefaultZ = 0.0f;
// W座標のデフォルト値
constexpr float kDefaultW = 1.0f;
// 最大深度
constexpr float kMaxDepth = 100.0f;
// アンカーポイントの基準値（左端/上端）
constexpr float kAnchorMin = 0.0f;
// アンカーポイントの基準値（右端/下端）
constexpr float kAnchorMax = 1.0f;
// 法線ベクトルのZ成分（手前向き）
constexpr float kNormalZ = -1.0f;

void Sprite::Initialize(SpriteCommon* spriteCommon, const std::string& textureFilePath)
{
	// 引数で受け取ってメンバ変数に記録する
	spriteCommon_ = spriteCommon;

	// テクスチャのSRVインデックスを取得
	textureIndex_ = TextureManager::GetInstance()->GetSRVIndex(textureFilePath);

	// 頂点データを作成する
	CreateVertexData();

	// スプライトのサイズをテクスチャと合わせる
	AdjustTextureSize();
}

void Sprite::Update()
{
	// 頂点データを更新する
	UpdateVertexData();

	// 座標変換行列を更新する
	UpdateMatrix();
}

void Sprite::UpdateWithMatrix(const Matrix4x4& world, const Matrix4x4& viewProjection)
{
	UpdateVertexData();
	transformationMatrixData_->WVP = Multiply(world, viewProjection);
	transformationMatrixData_->World = world;
}

void Sprite::SetRadialFill(float fill, float startAngle, bool clockwise)
{
	fillAmount_ = std::clamp(fill, 0.0f, 1.0f);
	fillStartAngle_ = startAngle;
	fillClockwise_ = clockwise;
}

void Sprite::DrawWithMatrix(const Matrix4x4& world, const Matrix4x4& viewProjection, FrameConstantAllocator* allocator)
{
	if (drawIndexCount_ == 0 || !allocator)
	{
		return;
	}
	struct SpriteTransform
	{
		Matrix4x4 WVP;
		Matrix4x4 World;
	};
	const auto allocation = allocator->Allocate(sizeof(SpriteTransform));
	if (!allocation.cpuAddress)
	{
		return;
	}
	*static_cast<SpriteTransform*>(allocation.cpuAddress) = { Multiply(world, viewProjection), world };

	auto* commandList = spriteCommon_->GetDXCommon()->GetCommandList();
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetIndexBuffer(&indexBufferView_);
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, allocation.gpuAddress);
	commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_));
	commandList->DrawIndexedInstanced(drawIndexCount_, 1, 0, 0, 0);
}

void Sprite::Draw()
{
	// 扇が空なら描かない
	if (drawIndexCount_ == 0)
	{
		return;
	}

	// VertexBufferViewを設定
	spriteCommon_->GetDXCommon()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);

	// IndexBufferViewを設定
	spriteCommon_->GetDXCommon()->GetCommandList()->IASetIndexBuffer(&indexBufferView_);

	// マテリアルCBufferの場所を設定
	spriteCommon_->GetDXCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	
	// 座標変換行列CBufferの場所を設定
	spriteCommon_->GetDXCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());

	// ShaderResourceViewの設定
	spriteCommon_->GetDXCommon()->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_));

	// 描画コマンドを発行（インデックス数分描画）
	spriteCommon_->GetDXCommon()->GetCommandList()->DrawIndexedInstanced(drawIndexCount_, 1, 0, 0, 0);
}

void Sprite::SetTexture(const std::string& filePath)
{
	// テクスチャをファイルパスで指定して読み込む
	textureIndex_ = TextureManager::GetInstance()->GetSRVIndex(filePath);

	// スプライトのサイズをテクスチャと合わせる
	AdjustTextureSize();

	// 行列を更新する
	UpdateMatrix();
}

void Sprite::CreateVertexData()
{
	// MaterialResourceを作成
	materialResource_ = spriteCommon_->GetDXCommon()->CreateBufferResource(sizeof(Material));

	// VertexResourceを作成（ラジアルワイプの最大数まで）
	vertexResource_ = spriteCommon_->GetDXCommon()->CreateBufferResource(sizeof(VertexData) * kMaxVertexCount);

	// IndexResourceを作成（ラジアルワイプの最大数まで）
	indexResource_ = spriteCommon_->GetDXCommon()->CreateBufferResource(sizeof(uint32_t) * kMaxIndexCount);

	// 座標変換行列リソースを作成
	wvpResource_ = spriteCommon_->GetDXCommon()->CreateBufferResource(sizeof(TransformationMatrix));

	// VertexBufferViewを設定
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * kMaxVertexCount;
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	// IndexBufferViewを設定
	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = sizeof(uint32_t) * kMaxIndexCount;
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	// MaterialResourceにデータを書き込むためのアドレスを取得
	materialResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&materialData_)
	);

	// マテリアルデータの初期値を設定
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->enableLighting = false;
	materialData_->uvTransform = MakeIdentity4x4();

	// VertexResourceにデータを書き込むためのアドレスを取得
	vertexResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&vertexData_)
	);

	// IndexResourceにデータを書き込むためのアドレスを取得
	indexResource_->Map(
		0, 
		nullptr, 
		reinterpret_cast<void**>(&indexData_)
	);

	// 座標変換行列にデータを書き込むためのアドレスを取得
	wvpResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&transformationMatrixData_)
	);

	// 座標変換行列の初期値を設定
	transformationMatrixData_->WVP = MakeIdentity4x4();
	transformationMatrixData_->World = MakeIdentity4x4();

	// インデックスデータを設定（三角形2つで四角形を構成）
	indexData_[0] = 0;		indexData_[1] = 1;		indexData_[2] = 2;
	indexData_[3] = 1;		indexData_[4] = 3;		indexData_[5] = 2;

}

void Sprite::UpdateVertexData()
{
	// ラジアルワイプ中は、扇形の頂点を作る
	if (fillAmount_ < 1.0f)
	{
		UpdateRadialVertexData();
		return;
	}
	// 通常の四角に戻す（扇のあとで、インデックスを四角用に直す）
	drawIndexCount_ = kIndexCount;
	indexData_[0] = 0;		indexData_[1] = 1;		indexData_[2] = 2;
	indexData_[3] = 1;		indexData_[4] = 3;		indexData_[5] = 2;

	// アンカーポイントを考慮して座標を計算
	float left = kAnchorMin - anchorPoint_.x;
	float right = kAnchorMax - anchorPoint_.x;
	float top = kAnchorMin - anchorPoint_.y;
	float bottom = kAnchorMax - anchorPoint_.y;

	// 左右反転
	if (isFlipX_)
	{
		float temp = left;
		left = right;
		right = temp;
	}

	// 上下反転
	if (isFlipY_)
	{
		float temp = top;
		top = bottom;
		bottom = temp;
	}

	// UV座標を計算（テクスチャの大きさは差し替えたときに覚えたものを使う）
	float tex_left = textureLeftTop_.x / textureDimension_.x;
	float tex_right = (textureLeftTop_.x + textureSize_.x) / textureDimension_.x;
	float tex_top = textureLeftTop_.y / textureDimension_.y;
	float tex_bottom = (textureLeftTop_.y + textureSize_.y) / textureDimension_.y;

	// 頂点データの座標を設定
	vertexData_[0].position = { left, bottom, kDefaultZ, kDefaultW };
	vertexData_[1].position = { left, top, kDefaultZ, kDefaultW };
	vertexData_[2].position = { right, bottom, kDefaultZ, kDefaultW };
	vertexData_[3].position = { right, top, kDefaultZ, kDefaultW };

	// 頂点データのテクスチャ座標を設定
	vertexData_[0].texcoord = { tex_left, tex_bottom };
	vertexData_[1].texcoord = { tex_left, tex_top };
	vertexData_[2].texcoord = { tex_right, tex_bottom };
	vertexData_[3].texcoord = { tex_right, tex_top };

	// 頂点データの法線を設定（手前向き）
	vertexData_[0].normal = { 0.0f, 0.0f, kNormalZ };
	vertexData_[1].normal = { 0.0f, 0.0f, kNormalZ };
	vertexData_[2].normal = { 0.0f, 0.0f, kNormalZ };
	vertexData_[3].normal = { 0.0f, 0.0f, kNormalZ };
}

void Sprite::UpdateRadialVertexData()
{
	if (fillAmount_ <= 0.0f)
	{
		drawIndexCount_ = 0;
		return;
	}

	// 0〜1 の単位四角の上で作る。角度は真上が 0 で、時計回りに増える。v は下向き
	// 中心から角度 theta の向きへ伸ばした線が、四角の縁に当たる点
	const auto edgePoint = [](float theta)
	{
		const float dx = std::sin(theta);
		const float dy = -std::cos(theta);
		const float t = 0.5f / (std::max)(std::fabs(dx), std::fabs(dy));
		return Vector2{ 0.5f + dx * t, 0.5f + dy * t };
	};
	const float direction = fillClockwise_ ? 1.0f : -1.0f;
	const float sweep = fillAmount_ * kTwoPi;

	// 縁の点。開始 → 範囲に入る角 → 終わり、の順
	Vector2 ring[6];
	int ringCount = 0;
	ring[ringCount++] = edgePoint(fillStartAngle_);

	// 範囲に入る角を、開始からの角度の小さい順に入れる
	struct Corner { float relative; Vector2 point; };
	Corner corners[4];
	int cornerCount = 0;
	const Vector2 kCornerPoints[4] = { { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f } };
	for (const Vector2& point : kCornerPoints)
	{
		// 角の向き（真上 0、時計回り）
		const float cornerAngle = std::atan2(point.x - 0.5f, -(point.y - 0.5f));
		float relative = direction * (cornerAngle - fillStartAngle_);
		relative = std::fmod(relative, kTwoPi);
		if (relative < 0.0f) { relative += kTwoPi; }
		if (relative > 0.0f && relative < sweep)
		{
			corners[cornerCount++] = { relative, point };
		}
	}
	std::sort(corners, corners + cornerCount, [](const Corner& a, const Corner& b) { return a.relative < b.relative; });
	for (int i = 0; i < cornerCount; ++i)
	{
		ring[ringCount++] = corners[i].point;
	}
	ring[ringCount++] = edgePoint(fillStartAngle_ + direction * sweep);

	// 頂点。0 が中心、1 以降が縁の点
	const float texLeft = textureLeftTop_.x / textureDimension_.x;
	const float texRight = (textureLeftTop_.x + textureSize_.x) / textureDimension_.x;
	const float texTop = textureLeftTop_.y / textureDimension_.y;
	const float texBottom = (textureLeftTop_.y + textureSize_.y) / textureDimension_.y;
	const auto setVertex = [&](int index, const Vector2& unit)
	{
		// 左右・上下反転は、位置を鏡に映す（UV はそのまま）
		const float x = (isFlipX_ ? 1.0f - unit.x : unit.x) - anchorPoint_.x;
		const float y = (isFlipY_ ? 1.0f - unit.y : unit.y) - anchorPoint_.y;
		vertexData_[index].position = { x, y, kDefaultZ, kDefaultW };
		vertexData_[index].texcoord = { texLeft + (texRight - texLeft) * unit.x, texTop + (texBottom - texTop) * unit.y };
		vertexData_[index].normal = { 0.0f, 0.0f, kNormalZ };
	};
	setVertex(0, { 0.5f, 0.5f });
	for (int i = 0; i < ringCount; ++i)
	{
		setVertex(1 + i, ring[i]);
	}

	// 中心から縁の隣り合う2点で三角形を作る（扇）
	uint32_t count = 0;
	for (int i = 0; i + 1 < ringCount; ++i)
	{
		indexData_[count++] = 0;
		indexData_[count++] = static_cast<uint32_t>(1 + i);
		indexData_[count++] = static_cast<uint32_t>(2 + i);
	}
	drawIndexCount_ = count;
}

void Sprite::UpdateMatrix()
{
	// Transform情報を構築
	Transform transform{
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f}
	};

	// 位置、回転、スケールを設定
	transform.translate = { position_.x, position_.y, kDefaultZ };
	transform.rotate = { 0.0f, 0.0f, rotation_ };
	transform.scale = { size_.x, size_.y, 1.0f };

	// ワールド行列を計算
	Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);

	// ビュー行列（2Dなので単位行列）
	Matrix4x4 viewMatrixSprite = MakeIdentity4x4();

	// 正射影行列を計算（1920x1080の基準解像度に固定）
	Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(
		0.0f, 0.0f, Sprite::kCoordinateWidth, Sprite::kCoordinateHeight, 0.0f, kMaxDepth);

	// WVP行列を計算して設定
	Matrix4x4 worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));
	transformationMatrixData_->WVP = worldViewProjectionMatrixSprite;
	transformationMatrixData_->World = worldMatrixSprite;
}

void Sprite::AdjustTextureSize()
{
	// テクスチャのメタデータを取得
	const DirectX::TexMetadata& metadata =
		TextureManager::GetInstance()->GetMetadata(textureIndex_);

	// テクスチャのサイズを取得
	textureSize_ = KCE::Vector2(
		static_cast<float>(metadata.width),
		static_cast<float>(metadata.height)
	);
	textureDimension_ = textureSize_;

	// 画像サイズをテクスチャサイズに合わせる
	size_ = textureSize_;
}
} // namespace KCE
