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
#include "math/Vector4.h"
#include "base/GraphicsTypes.h"
#include <d3d12.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace KCE
{
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
	 * @brief ビルボード有効化状態を取得
	 * @return ビルボードが有効な場合true
	 */
	virtual bool GetBillboard() const { return true; }

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
	virtual void SetEmissiveTexture(const std::string& /*texturePath*/) {}
	virtual std::string GetEmissiveTexturePath() const { return ""; }

	/**
	 * @brief GPU描画モードを設定
	 * @param enable GPU描画モード有効化フラグ
	 * @param srvIndex GPUパーティクルバッファのSRVインデックス
	 * @param count GPUパーティクル数
	 */
	virtual void SetGPUMode(bool enable, uint32_t srvIndex, uint32_t count, ID3D12Resource* drawArguments = nullptr) { (void)enable; (void)srvIndex; (void)count; (void)drawArguments; }
	virtual void SetGPURibbonMode(bool enable, ID3D12Resource* vertexBuffer, ID3D12Resource* drawArguments, uint32_t maxVertices)
	{ (void)enable; (void)vertexBuffer; (void)drawArguments; (void)maxVertices; }

	/**
	 * @brief レンダラーの状態をリセット（プール再利用時など）
	 */
	virtual void Reset() {}

	/**
	 * @brief ブレンドモードを設定
	 * @param mode ブレンドモード
	 */
	virtual void SetBlendMode(BlendMode mode) { if (IsValidBlendMode(mode)) blendMode_ = mode; }

	/**
	 * @brief ブレンドモードを取得
	 * @return ブレンドモード
	 */
	BlendMode GetBlendMode() const { return blendMode_; }

	/**
	 * @brief ティントカラーを設定
	 * @param color ティントカラー（RGBA）
	 */
	virtual void SetTintColor(const Vector4& /*color*/) {}

	/**
	 * @brief ティントカラーを取得
	 * @return ティントカラー（RGBA）
	 */
	virtual Vector4 GetTintColor() const { return { 1.0f, 1.0f, 1.0f, 1.0f }; }

	void SetEmissiveEnabled(bool enabled) { emissiveSettings_.enabled = enabled; }
	void SetEmissiveSource(EmissiveSource source) {
		if (static_cast<uint32_t>(source) <= static_cast<uint32_t>(EmissiveSource::EmissiveTexture)) emissiveSettings_.source = source;
	}
	void SetEmissiveColor(const Vector3& color) {
		if (!std::isfinite(color.x) || !std::isfinite(color.y) || !std::isfinite(color.z)) return;
		emissiveSettings_.color = {
			(std::clamp)(color.x, 0.0f, 16.0f),
			(std::clamp)(color.y, 0.0f, 16.0f),
			(std::clamp)(color.z, 0.0f, 16.0f)
		};
	}
	void SetEmissiveIntensity(float intensity) { if (std::isfinite(intensity)) emissiveSettings_.intensity = (std::clamp)(intensity, 0.0f, 64.0f); }
	void SetBloomContribution(float contribution) { if (std::isfinite(contribution)) emissiveSettings_.bloomContribution = (std::clamp)(contribution, 0.0f, 1.0f); }
	void SetEmissiveSettings(const EmissiveSettings& settings) {
		EmissiveSettings normalized;
		if (TryNormalizeEmissiveSettings(settings, normalized)) emissiveSettings_ = normalized;
	}
	const EmissiveSettings& GetEmissiveSettings() const { return emissiveSettings_; }
#ifdef _DEBUG
	/** GPU shaderの異常値防御を検証するためだけの非正規入力。製品APIでは使用禁止。 */
	void DebugInjectRawEmissiveSettings(const EmissiveSettings& settings) { emissiveSettings_ = settings; }
#endif

protected:
	BlendMode blendMode_ = BlendMode::Alpha;
	EmissiveSettings emissiveSettings_{};
};
} // namespace KCE
