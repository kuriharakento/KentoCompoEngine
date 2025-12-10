#pragma once
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
	virtual void Initialize(const std::string& texturePath) = 0;
	virtual void Update(const std::vector<Particle>& particles, CameraManager* camera) = 0;
	virtual void Draw(DirectXCommon* dxCommon, SrvManager* srvManager) = 0;
	virtual RendererType GetType() const = 0;
	// テクスチャ設定
	virtual void SetTexture(const std::string& /*texturePath*/) {}

	/**
	 * @brief GPU描画モードを設定
	 */
	virtual void SetGPUMode(bool enable, uint32_t srvIndex, uint32_t count) { (void)enable; (void)srvIndex; (void)count; }

	virtual void SetBlendMode(BlendMode mode) { blendMode_ = mode; }
	BlendMode GetBlendMode() const { return blendMode_; }

protected:
	BlendMode blendMode_ = BlendMode::Alpha;
};
