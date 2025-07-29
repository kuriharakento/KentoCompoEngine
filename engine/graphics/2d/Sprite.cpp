#include "Sprite.h"

#include "SpriteCommon.h"
#include "manager/graphics/TextureManager.h"

void Sprite::Initialize(SpriteCommon* spriteCommon, std::string textureFilePath)
{
	//引数で受け取ってメンバ変数に記録する
	spriteCommon_ = spriteCommon;

	//単位行列を書き込んでおく
	textureIndex_ = TextureManager::GetInstance()->GetSRVIndex(textureFilePath);

	//頂点データを作成する
	CreateVertexData();

	//スプライトのサイズをテクスチャと合わせる
	AdjustTextureSize();
}

void Sprite::Update()
{
	//頂点データを更新する
	UpdateVertexData();

	//座標変換行列を更新する
	UpdateMatrix();
}

void Sprite::Draw()
{
	/*--------------[ VertexBufferViewを設定 ]-----------------*/

	spriteCommon_->GetDXCommon()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);

	/*--------------[ IndexBufferViewを設定 ]-----------------*/

	spriteCommon_->GetDXCommon()->GetCommandList()->IASetIndexBuffer(&indexBufferView_);

	/*--------------[ マテリアルCBufferの場所を設定 ]-----------------*/

	spriteCommon_->GetDXCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	
	/*--------------[ 座標変換行列CBufferの場所を設定 ]-----------------*/

	spriteCommon_->GetDXCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());

	/*--------------[ ShaderResourceViewの設定 ]-----------------*/

	spriteCommon_->GetDXCommon()->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_));

	/*--------------[ ライティングの設定。今はしない ]-----------------*/

	//spriteCommon_->GetDXCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());

	/*--------------[ 描画(DrawCall//ドローコール) ]-----------------*/

	spriteCommon_->GetDXCommon()->GetCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void Sprite::SetTexture(std::string filePath)
{
	//テクスチャをファイルパスで指定して読み込む
	textureIndex_ = TextureManager::GetInstance()->GetSRVIndex(filePath);
	//スプライトのサイズをテクスチャと合わせる
	AdjustTextureSize();
	//行列を更新する
	UpdateMatrix();
}

void Sprite::CreateVertexData()
{
	/*--------------[ MaterialResourceを作る ]-----------------*/

	materialResource_ = spriteCommon_->GetDXCommon()->CreateBufferResource(sizeof(Material));

	/*--------------[ VertexResourceを作る ]-----------------*/
	
	vertexResource_ = spriteCommon_->GetDXCommon()->CreateBufferResource(sizeof(VertexData) * 4);

	/*--------------[ IndexResourceを作る ]-----------------*/

	indexResource_ = spriteCommon_->GetDXCommon()->CreateBufferResource(sizeof(uint32_t) * 6);

	/*--------------[ 座標変換行列リソースを作る ]-----------------*/

	wvpResource_ = spriteCommon_->GetDXCommon()->CreateBufferResource(sizeof(TransformationMatrix));

	/*--------------[ VertexBufferViewを作成する（値を設定するだけ） ]-----------------*/

	//リソースの先頭アドレスから使う
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	//使用するリソースのサイズは頂点３つ分のサイズ
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * 4;
	//1頂点当たりのサイズ
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	/*--------------[ IndexBufferViewを作成する（値を設定するだけ） ]-----------------*/

	//リソースの先頭のアドレスから使う
	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	//使用するリソースのサイズをインデックス6つ分のサイズ
	indexBufferView_.SizeInBytes = sizeof(uint32_t) * 6;
	//インデックスはuint32_tとする
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	/*--------------[ MaterialResourceにデータを書き込むためのアドレスを取得してvertexDataに割り当てる ]-----------------*/

	materialResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&materialData_)
	);

	/*--------------[ マテリアルデータの初期値を書き込む ]-----------------*/

	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->enableLighting = false;
	materialData_->uvTransform = MakeIdentity4x4();

	/*--------------[ VertexResourceにデータを書き込むためのアドレスを取得してvertexDataに割り当てる ]-----------------*/

	vertexResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&vertexData_)
	);

	/*--------------[ IndexResourceにデータを書き込むためのアドレスを取得してindexDataに割り当てる ]-----------------*/

	indexResource_->Map(
		0, 
		nullptr, 
		reinterpret_cast<void**>(&indexData_)
	);

	/*--------------[ 座標変換行列にデータを書き込むためのアドレスを取得してindexDataに割り当てる ]-----------------*/

	wvpResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&transformationMatrixData_)
	);

	/*--------------[ 座標変換行列の初期値を書き込む ]-----------------*/

	transformationMatrixData_->WVP = MakeIdentity4x4();
	transformationMatrixData_->World = MakeIdentity4x4();

	/*--------------[ インデックスリソースにデータを書き込む ]-----------------*/

	indexData_[0] = 0;		indexData_[1] = 1;		indexData_[2] = 2;
	indexData_[3] = 1;		indexData_[4] = 3;		indexData_[5] = 2;

}

void Sprite::UpdateVertexData()
{
	/*--------------[ 頂点データに書き込む ]-----------------*/

	//アンカーポイントを考慮して座標を計算
	float left = 0.0f - anchorPoint_.x;
	float right = 1.0f - anchorPoint_.x;
	float top = 0.0f - anchorPoint_.y;
	float bottom = 1.0f - anchorPoint_.y;

	//左右反転
	if (isFlipX_)
	{
		float temp = left;
		left = right;
		right = temp;
	}

	//上下反転
	if (isFlipY_)
	{
		float temp = top;
		top = bottom;
		bottom = temp;
	}

	//テクスチャのメタデータを取得
	const DirectX::TexMetadata& metadata =
		TextureManager::GetInstance()->GetMetadata(textureIndex_);
	float tex_left = textureLeftTop_.x / metadata.width;
	float tex_right = (textureLeftTop_.x + textureSize_.x) / metadata.width;
	float tex_top = textureLeftTop_.y / metadata.height;
	float tex_bottom = (textureLeftTop_.y + textureSize_.y) / metadata.height;

	//頂点データの座標
	vertexData_[0].position = { left,bottom,0.0f,1.0f };
	vertexData_[1].position = { left,top,0.0f,1.0f };
	vertexData_[2].position = { right,bottom,0.0f,1.0f };
	vertexData_[3].position = { right,top,0.0f,1.0f };

	//頂点データのテクスチャ座標
	vertexData_[0].texcoord = { tex_left,tex_bottom };
	vertexData_[1].texcoord = { tex_left,tex_top };
	vertexData_[2].texcoord = { tex_right,tex_bottom };
	vertexData_[3].texcoord = { tex_right,tex_top };

	//頂点データの法線
	vertexData_[0].normal = { 0.0f,0.0f,-1.0f };
	vertexData_[1].normal = { 0.0f,0.0f,-1.0f };
	vertexData_[3].normal = { 0.0f,0.0f,-1.0f };	
	vertexData_[3].normal = { 0.0f,0.0f,-1.0f };
}

void Sprite::UpdateMatrix()
{
	/*--------------[ Transform情報 ]-----------------*/

	Transform transform{
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f}
	};

	transform.translate = { position_.x,position_.y,0.0f };
	transform.rotate = { 0.0f,0.0f,rotation_ };
	transform.scale = { size_.x,size_.y,1.0f };

	Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
	Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0.0f, 100.0f);
	Matrix4x4 worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));
	transformationMatrixData_->WVP = worldViewProjectionMatrixSprite;
	transformationMatrixData_->World = worldMatrixSprite;
}

void Sprite::AdjustTextureSize()
{
	//テクスチャのメタデータを取得
	const DirectX::TexMetadata& metadata =
		TextureManager::GetInstance()->GetMetadata(textureIndex_);

	//テクスチャのサイズを取得
	textureSize_ = Vector2(
		static_cast<float>(metadata.width), 
		static_cast<float>(metadata.height)
	);

	//画像サイズをテクスチャサイズに合わせる
	size_ = textureSize_;
}

