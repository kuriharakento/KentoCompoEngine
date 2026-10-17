#include "AnimatedSprite.h"

#include "base/Logger.h"
#include "manager/graphics/TextureManager.h"

namespace KCE
{
bool AnimatedSprite::Initialize(SpriteCommon* spriteCommon, const std::string& clipName)
{
	const std::string path = SpriteAnimationClip::BuildFilePath(clipName);
	SpriteAnimationClip clip;
	if (!clip.LoadFromFile(path))
	{
		Logger::Log("スプライトアニメーションを読み込めませんでした: " + path + "\n", Logger::LogLevel::Error);
		return false;
	}
	return Initialize(spriteCommon, clip);
}

bool AnimatedSprite::Initialize(SpriteCommon* spriteCommon, const SpriteAnimationClip& clip)
{
	if (clip.GetKeys().empty())
	{
		Logger::Log("スプライトアニメーションにキーが無いため初期化できませんでした。\n", Logger::LogLevel::Error);
		return false;
	}

	// 先に全部読んでおく。再生中に読み込みが走ってカクつかないようにする
	for (const SpriteAnimationKey& key : clip.GetKeys())
	{
		TextureManager::GetInstance()->LoadTexture(key.texturePath);
	}

	sprite_.Initialize(spriteCommon, clip.GetKeys().front().texturePath);
	fixedSize_ = sprite_.GetSize();
	animator_.Initialize(&sprite_);
	animator_.SetClip(clip);
	isInitialized_ = true;
	return true;
}

void AnimatedSprite::Update(float dt)
{
	if (!isInitialized_)
	{
		return;
	}

	animator_.Update(dt);
	// SetTexture が大きさを画像に合わせ直すので、保つ設定ならここで戻す
	if (keepSize_)
	{
		sprite_.SetSize(fixedSize_);
	}
	sprite_.Update();
}

void AnimatedSprite::Draw()
{
	if (!isInitialized_)
	{
		return;
	}
	sprite_.Draw();
}

void AnimatedSprite::SetSize(const KCE::Vector2& size)
{
	fixedSize_ = size;
	sprite_.SetSize(size);
}

void AnimatedSprite::SetLooping(bool isLooping)
{
	animator_.SetLooping(isLooping);
}
} // namespace KCE
