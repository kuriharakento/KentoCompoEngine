#pragma once
#include <cstdint>
#include <d3d12.h>
#include <string>
#include <wrl.h>

#include "base/GraphicsTypes.h"

//ポインタが必要なので前方宣言
class SpriteCommon;

//スプライト
class Sprite
{
public: //メンバ関数

	/**
	 * \brief 初期化
	 * \param spriteCommon 
	 */
	void Initialize(SpriteCommon* spriteCommon,std::string textureFilePath);

	/**
	 * \brief 更新処理
	 */
	void Update();

	/**
	 * \brief 描画
	 */
	void Draw();

public: //アクセッサ
	/*---------------[ ゲッター ]---------------*/
	
	/// \brief 座標の取得
	const Vector2& GetPosition() const { return position_; }

	//// \brief 回転の取得
	float GetRotation() const { return rotation_; }

	//// \brief 色の取得
	const Vector4& GetColor() const { return materialData_->color; }

	//// \brief サイズの取得
	const Vector2& GetSize() const { return size_; }

	//// \brief アンカーポイントの取得
	const Vector2& GetAnchorPoint() const { return anchorPoint_; }

	/// \brief 左右フリップの取得
	const bool GetFlipX() const { return isFlipX_; }

	/// \brief 上下フリップの取得
	const bool GetFlipY() const { return isFlipY_; }

	/// \brief テクスチャ左上座標の取得
	const Vector2& GetTextureLeftTop() const { return textureLeftTop_; }

	/// \brief テクスチャ切り出しサイズの取得
	const Vector2& GetTextureSize() const { return textureSize_; }

	/*---------------[ セッター ]---------------*/

	/// \brief テクスチャの差し替え
	void SetTexture(std::string filePath);

	/// \brief 座標の設定
	void SetPosition(const Vector2& position) { position_ = position; }

	/// \brief 回転の設定
	void SetRotation(float rotation) { rotation_ = rotation; }

	/// \brief 色の設定
	void SetColor(const Vector4& color) { materialData_->color = color; }

	//// \brief サイズの設定
	void SetSize(const Vector2& size) { size_ = size; }

	/// \brief アンカーポイントの設定
	void SetAnchorPoint(const Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }

	/// \brief 左右フリップの設定
	void SetFlipX(bool isFlipX) { isFlipX_ = isFlipX; }

	/// \brief 上下フリップの設定
	void SetFlipY(bool isFlipY) { isFlipY_ = isFlipY; }

	/// \brief テクスチャ左上座標の設定
	void SetTextureLeftTop(const Vector2& textureLeftTop) { textureLeftTop_ = textureLeftTop; }

	/// \brief テクスチャ切り出しサイズの設定
	void SetTextureSize(const Vector2& textureSize) { textureSize_ = textureSize; }

private: //メンバ関数
	/// \brief 頂点データ作成
	void CreateVertexData();

	/// \brief 頂点データの更新
	void UpdateVertexData();

	/// \brief 行列の更新
	void UpdateMatrix();

	/**
	 * \brief テクスチャサイズをイメージに合わせる
	 */
	void AdjustTextureSize();

private: //描画用変数
	SpriteCommon* spriteCommon_ = nullptr;

	//バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_;
	//バッファリソース内のデータを指すポインタ
	Material* materialData_ = nullptr;
	VertexData* vertexData_ = nullptr;
	uint32_t* indexData_ = nullptr;
	TransformationMatrix* transformationMatrixData_ = nullptr;
	//バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_;

private: //メンバ変数
	//テクスチャ番号
	uint32_t textureIndex_ = 0;

	//座標
	Vector2 position_ = { 0.0f,0.0f };

	//回転
	float rotation_ = 0.0f;

	//サイズ
	Vector2 size_ = { 0.0f,0.0f };

	//アンカーポイント
	Vector2 anchorPoint_ = { 0.0f,0.0f };

	//左右フリップ
	bool isFlipX_ = false;

	//上下フリップ
	bool isFlipY_ = false;

	//テクスチャ左上座標
	Vector2 textureLeftTop_ = { 0.0f,0.0f };

	//テクスチャ切り出しサイズ
	Vector2 textureSize_ = { 0.0f,0.0f };

};

