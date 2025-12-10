#pragma once
#include "math/Vector3.h"
#include <cstdint>

class DirectXCommon;

/**
 * @brief GPUパーティクルシミュレーター
 */
class GPUSimulator
{
public:
	GPUSimulator() = default;
	~GPUSimulator() = default;

	void Initialize(DirectXCommon* dxCommon, uint32_t maxParticles);
	void SpawnParticles(uint32_t count);
	void Dispatch(float deltaTime);
	void SetEmitterPosition(const Vector3& position);

private:
	DirectXCommon* dxCommon_ = nullptr;
	uint32_t maxParticles_ = 0;
	Vector3 emitterPosition_ = {};
};
