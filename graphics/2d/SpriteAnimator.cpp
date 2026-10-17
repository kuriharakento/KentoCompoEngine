#include "SpriteAnimator.h"

#include <algorithm>

#include "graphics/2d/Sprite.h"

namespace KCE
{
void SpriteAnimator::Initialize(Sprite* sprite)
{
	sprite_ = sprite;
	appliedKeyIndex_ = -1;
}

void SpriteAnimator::SetClip(const SpriteAnimationClip& clip)
{
	clip_ = clip;
	time_ = 0.0f;
	appliedKeyIndex_ = -1;
	Apply();
}

void SpriteAnimator::Stop()
{
	isPlaying_ = false;
	time_ = 0.0f;
	Apply();
}

void SpriteAnimator::Update(float dt)
{
	if (!isPlaying_)
	{
		return;
	}

	time_ += (std::max)(dt, 0.0f) * speed_;
	// 非ループで末尾に着いたら止める。丸める前の時刻で判定する
	if (!clip_.IsLooping() && time_ >= clip_.GetDuration())
	{
		isPlaying_ = false;
	}
	// 折り返し（ループ）か末尾での丸め（非ループ）。時刻が育ちすぎないようにここで収める
	time_ = clip_.WrapTime(time_);
	Apply();
}

void SpriteAnimator::SetTime(float time)
{
	time_ = clip_.WrapTime(time);
	Apply();
}

void SpriteAnimator::Apply()
{
	if (!sprite_)
	{
		return;
	}

	const int keyIndex = clip_.FindKeyIndex(time_);
	if (keyIndex < 0 || keyIndex == appliedKeyIndex_)
	{
		return;
	}
	appliedKeyIndex_ = keyIndex;
	sprite_->SetTexture(clip_.GetKeys()[static_cast<size_t>(keyIndex)].texturePath);
}
} // namespace KCE
