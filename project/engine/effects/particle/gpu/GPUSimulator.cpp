#include "GPUSimulator.h"
#include "base/DirectXCommon.h"

void GPUSimulator::Initialize(DirectXCommon* dxCommon, uint32_t maxParticles)
{
	dxCommon_ = dxCommon;
	maxParticles_ = maxParticles;
	// TODO: GPU バッファとCompute Shaderの初期化
}

void GPUSimulator::SpawnParticles(uint32_t count)
{
	(void)count;
}

void GPUSimulator::Dispatch(float deltaTime)
{
	(void)deltaTime;
}

void GPUSimulator::SetEmitterPosition(const Vector3& position)
{
	emitterPosition_ = position;
}
