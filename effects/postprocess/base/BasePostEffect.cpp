#include "BasePostEffect.h"
#include "DirectXTex/d3dx12.h"
#include <cassert>

namespace KCE
{
// コンストラクタ：エフェクトを無効状態で初期化
BasePostEffect::BasePostEffect() : enabled_(false), isDirty_(true) {}

// デストラクタ
BasePostEffect::~BasePostEffect() {}


void BasePostEffect::SetEnabled(bool enabled)
{
	// 値が変更された場合のみ更新
	if (enabled_ != enabled)
	{
		enabled_ = enabled;
		isDirty_ = true;
	}
}
} // namespace KCE
