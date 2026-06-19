#pragma once
/**
 * @file IRenderer.h
 * @brief パーティクルレンダラーインターフェース
 * 
 * すべてのパーティクルレンダラーの基底クラス。
 * 描画方式（Sprite, Ribbon, Mesh）の共通インターフェースを定義。
 */
#include "effects/particle/Particle.h"
#include "effects/particle/ParticleTypes.h"
#include <vector>

class CameraManager;
class DirectXCommon;
class SrvManager;

/**
 * @brief パーティクルレンダラー基底インターフェース
 */
class IRenderer
{
public:
	virtual ~IRenderer() = default;

	/**
	 * @brief 初期化
	 * @param texturePath テクスチャファイルパス
	 */
	virtual void Initialize(const std::string& texturePath) = 0;

	/**
	 * @brief パーティクルデータを更新
	 * @param particles パーティクルリスト
	 * @param camera カメラマネージャー
	 */
	virtual void Update(const std::vector<Particle>& particles, CameraManager* camera) = 0;

	/**
	 * @brief 描画
	 * @param dxCommon DirectXCommonポインタ
	 * @param srvManager SrvManagerポインタ
	 */
	virtual void Draw(DirectXCommon* dxCommon, SrvManager* srvManager) = 0;

	/**
	 * @brief レンダラータイプを取得
	 * @return レンダラータイプ
	 */
	virtual RendererType GetType() const = 0;

	/**
	 * @brief テクスチャを設定
	 * @param texturePath テクスチャファイルパス
	 */
	virtual void SetTexture(const std::string& /*texturePath*/) {}

	/**
	 * @brief テクスチャパスを取得
	 * @return テクスチャファイルパス
	 */
	virtual std::string GetTexturePath() const { return ""; }

	/**
	 * @brief GPU描画モードを設定
	 * @param enable GPU描画モード有効化フラグ
	 * @param srvIndex GPUパーティクルバッファのSRVインデックス
	 * @param count GPUパーティクル数
	 */
	virtual void SetGPUMode(bool enable, uint32_t srvIndex, uint32_t count) { (void)enable; (void)srvIndex; (void)count; }

	/**
	 * @brief ブレンドモードを設定
	 * @param mode ブレンドモード
	 */
	virtual void SetBlendMode(BlendMode mode) { blendMode_ = mode; }

	/**
	 * @brief ブレンドモードを取得
	 * @return ブレンドモード
	 */
	BlendMode GetBlendMode() const { return blendMode_; }

protected:
	BlendMode blendMode_ = BlendMode::Alpha;
};
