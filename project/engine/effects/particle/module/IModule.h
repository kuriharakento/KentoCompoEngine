#pragma once
#include "effects/particle/ParticleTypes.h"

struct ParticleContext;

/**
 * @brief パーティクルモジュールインターフェース
 */
class IModule
{
public:
	virtual ~IModule() = default;
	virtual void Execute(ParticleContext& context) = 0;
	virtual ModulePhase GetPhase() const = 0;
	virtual const char* GetName() const = 0;
};
