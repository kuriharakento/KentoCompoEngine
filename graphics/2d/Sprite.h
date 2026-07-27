#pragma once
#include <cstdint>
#include <d3d12.h>
#include <string>
#include <wrl.h>

#include "base/GraphicsTypes.h"

// ポインタが必要なので前方宣言
class SpriteCommon;

/**
 * @brief スプライトクラス
 * @details 2Dテクスチャを画面に描画するためのクラス。
 *          頂点バッファ、インデックスバッファ、マテリアルなどを管理する。
 */
class Sprite
{
public:
	static constexpr float kCoordinateWidth = 1920.0f;
	static constexpr float kCoordinateHeight = 1080.0f;

public: // メンバ関数

	/**
	 * @brief 初期化
	 * @param spriteCommon スプライト共通部へのポインタ
	 * @param textureFilePath テクスチャのファイルパス
	 */
	void Initialize(SpriteCommon* spriteCommon,const std::string& textureFilePath);

	/**
	 * @brief 更新処理
	 * @details 頂点データと変換行列を更新する
	 */
	void Update();

	/**
	 * @brief 描画
	 * @details コマンドリストに描画コマンドを発行する
	 */
	void Draw();

public: // アクセッサ
	/*---------------[ ゲッター ]---------------*/
	
	/**
	 * @brief 座標の取得
	 * @return 現在の座標
	 */
	const KCE::Vector2& GetPosition() const { return position_; }

	/**
	 * @brief 回転の取得
	 * @return 現在の回転角度（ラジアン）
	 */
	float GetRotation() const { return rotation_; }

	/**
	 * @brief 色の取得
	 * @return 現在の色（RGBA）
	 */
	const Vector4& GetColor() const { return materialData_->color; }

	/**
	 * @brief サイズの取得
	 * @return 現在のサイズ（幅、高さ）
	 */
	const KCE::Vector2& GetSize() const { return size_; }

	/**
	 * @brief アンカーポイントの取得
	 * @return 現在のアンカーポイント（0.0-1.0）
	 */
	const KCE::Vector2& GetAnchorPoint() const { return anchorPoint_; }

	/**
	 * @brief 左右フリップの取得
	 * @return 左右反転フラグ
	 */
	const bool GetFlipX() const { return isFlipX_; }

	/**
	 * @brief 上下フリップの取得
	 * @return 上下反転フラグ
	 */
	const bool GetFlipY() const { return isFlipY_; }

	/**
	 * @brief テクスチャ左上座標の取得
	 * @return テクスチャ内の切り出し開始位置
	 */
	const KCE::Vector2& GetTextureLeftTop() const { return textureLeftTop_; }

	/**
	 * @brief テクスチャ切り出しサイズの取得
	 * @return テクスチャの切り出しサイズ
	 */
	const KCE::Vector2& GetTextureSize() const { return textureSize_; }

	/*---------------[ セッター ]---------------*/

	/**
	 * @brief テクスチャの差し替え
	 * @param filePath 新しいテクスチャのファイルパス
	 */
	void SetTexture(const std::string& filePath);

	/**
	 * @brief 座標の設定
	 * @param position 新しい座標
	 */
	void SetPosition(const KCE::Vector2& position) { position_ = position; }

	/**
	 * @brief 回転の設定
	 * @param rotation 新しい回転角度（ラジアン）
	 */
	void SetRotation(float rotation) { rotation_ = rotation; }

	/**
	 * @brief 色の設定
	 * @param color 新しい色（RGBA）
	 */
	void SetColor(const Vector4& color) { materialData_->color = color; }

	/**
	 * @brief サイズの設定
	 * @param size 新しいサイズ（幅、高さ）
	 */
	void SetSize(const KCE::Vector2& size) { size_ = size; }

	/**
	 * @brief アンカーポイントの設定
	 * @param anchorPoint 新しいアンカーポイント（0.0-1.0）
	 */
	void SetAnchorPoint(const KCE::Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }

	/**
	 * @brief 左右フリップの設定
	 * @param isFlipX 左右反転フラグ
	 */
	void SetFlipX(bool isFlipX) { isFlipX_ = isFlipX; }

	/**
	 * @brief 上下フリップの設定
	 * @param isFlipY 上下反転フラグ
	 */
	void SetFlipY(bool isFlipY) { isFlipY_ = isFlipY; }

	/**
	 * @brief テクスチャ左上座標の設定
	 * @param textureLeftTop テクスチャ内の切り出し開始位置
	 */
	void SetTextureLeftTop(const KCE::Vector2& textureLeftTop) { textureLeftTop_ = textureLeftTop; }

	/**
	 * @brief テクスチャ切り出しサイズの設定
	 * @param textureSize テクスチャの切り出しサイズ
	 */
	void SetTextureSize(const KCE::Vector2& textureSize) { textureSize_ = textureSize; }

private: // メンバ関数
	/**
	 * @brief 頂点データ作成
	 * @details 頂点バッファ、インデックスバッファ、マテリアルリソースを生成する
	 */
	void CreateVertexData();

	/**
	 * @brief 頂点データの更新
	 * @details アンカーポイント、フリップ、テクスチャ座標を反映する
	 */
	void UpdateVertexData();

	/**
	 * @brief 行列の更新
	 * @details ワールド行列とビュープロジェクション行列を計算する
	 */
	void UpdateMatrix();

	/**
	 * @brief テクスチャサイズをイメージに合わせる
	 * @details テクスチャのメタデータからサイズを取得して設定する
	 */
	void AdjustTextureSize();

private: // 描画用変数
	// スプライト共通部へのポインタ
	SpriteCommon* spriteCommon_ = nullptr;

	// マテリアル用バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	// 頂点用バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	// インデックス用バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
	// 座標変換行列用バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_;

	// マテリアルデータへのポインタ
	Material* materialData_ = nullptr;
	// 頂点データへのポインタ
	VertexData* vertexData_ = nullptr;
	// インデックスデータへのポインタ
	uint32_t* indexData_ = nullptr;
	// 変換行列データへのポインタ
	TransformationMatrix* transformationMatrixData_ = nullptr;

	// 頂点バッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
	// インデックスバッファビュー
	D3D12_INDEX_BUFFER_VIEW indexBufferView_;

private: // メンバ変数
	// テクスチャ番号
	uint32_t textureIndex_ = 0;

	// 座標
	KCE::Vector2 position_ = { 0.0f,0.0f };

	// 回転
	float rotation_ = 0.0f;

	// サイズ
	KCE::Vector2 size_ = { 0.0f,0.0f };

	// アンカーポイント
	KCE::Vector2 anchorPoint_ = { 0.0f,0.0f };

	// 左右フリップ
	bool isFlipX_ = false;

	// 上下フリップ
	bool isFlipY_ = false;

	// テクスチャ左上座標
	KCE::Vector2 textureLeftTop_ = { 0.0f,0.0f };

	// テクスチャ切り出しサイズ
	KCE::Vector2 textureSize_ = { 0.0f,0.0f };
};

