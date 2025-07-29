#include "BasePostEffect.h"
#include "DirectXTex/d3dx12.h"
#include <cassert>

BasePostEffect::BasePostEffect() : enabled_(false), isDirty_(true) {}

BasePostEffect::~BasePostEffect() {}


void BasePostEffect::SetEnabled(bool enabled)
{
	if (enabled_ != enabled)
	{
		enabled_ =
		isDirty_ = true; // パラメータが変更されたことを示す
	}
}