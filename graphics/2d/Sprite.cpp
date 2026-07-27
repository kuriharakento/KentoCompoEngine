#include "Sprite.h"

#include "SpriteCommon.h"
#include "manager/graphics/TextureManager.h"

// スプライトの頂点数
constexpr int kVertexCount = 4;
// スプライトのインデックス数
constexpr int kIndexCount = 6;
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

void Sprite::Draw()
{
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
	spriteCommon_->GetDXCommon()->GetCommandList()->DrawIndexedInstanced(kIndexCount, 1, 0, 0, 0);
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

	// VertexResourceを作成（頂点数分）
	vertexResource_ = spriteCommon_->GetDXCommon()->CreateBufferResource(sizeof(VertexData) * kVertexCount);

	// IndexResourceを作成（インデックス数分）
	indexResource_ = spriteCommon_->GetDXCommon()->CreateBufferResource(sizeof(uint32_t) * kIndexCount);

	// 座標変換行列リソースを作成
	wvpResource_ = spriteCommon_->GetDXCommon()->CreateBufferResource(sizeof(TransformationMatrix));

	// VertexBufferViewを設定
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * kVertexCount;
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	// IndexBufferViewを設定
	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = sizeof(uint32_t) * kIndexCount;
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

	// テクスチャのメタデータを取得してUV座標を計算
	const DirectX::TexMetadata& metadata =
		TextureManager::GetInstance()->GetMetadata(textureIndex_);
	float tex_left = textureLeftTop_.x / metadata.width;
	float tex_right = (textureLeftTop_.x + textureSize_.x) / metadata.width;
	float tex_top = textureLeftTop_.y / metadata.height;
	float tex_bottom = (textureLeftTop_.y + textureSize_.y) / metadata.height;

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

	// 画像サイズをテクスチャサイズに合わせる
	size_ = textureSize_;
}


