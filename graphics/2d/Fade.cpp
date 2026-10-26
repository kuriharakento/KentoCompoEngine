#include "Fade.h"

#include <algorithm>

#include "base/WinApp.h"
#include "graphics/2d/SpriteCommon.h"
#include "manager/graphics/TextureManager.h"
#include "math/VectorColorCodes.h"
#include "time/TimeManager.h"

namespace KCE
{
	namespace
	{
		constexpr const char* kFadeTextureName = "textures/white1x1.png";
	} // namespace

	void Fade::Initialize(SpriteCommon* spriteCommon)
	{
		TextureManager::GetInstance()->LoadTexture(kFadeTextureName, ResourceLifetime::Resident);

		sprite_ = std::make_unique<Sprite>();
		sprite_->Initialize(spriteCommon, kFadeTextureName);
		sprite_->SetColor(VectorColorCodes::Black);
		sprite_->SetSize({static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight)});
		sprite_->Update();

		mode_ = Mode::None;
		alpha_ = 0.0f;
		ApplyAlpha();
	}

	void Fade::Update()
	{
		if (mode_ == Mode::None)
		{
			return;
		}

		timer_.Update(KCE::TimeManager::GetInstance().GetDeltaTime(KCE::UIClock()));
		ApplyAlpha();
	}

	void Fade::Draw()
	{
		if (alpha_ <= 0.0f)
		{
			return;
		}
		sprite_->Draw();
	}

	void Fade::StartFadeIn(float duration)
	{
		Begin(Mode::FadeIn, duration);
	}

	void Fade::StartFadeOut(float duration)
	{
		Begin(Mode::FadeOut, duration);
	}

	void Fade::Begin(Mode mode, float duration)
	{
		mode_ = mode;
		timer_.SetDuration(duration);
		timer_.Start();
		ApplyAlpha();
	}

	void Fade::ApplyAlpha()
	{
		float duration = timer_.GetDuration();
		const float progress = (duration <= 0.0f || timer_.IsFinished()) ? 1.0f : std::clamp(timer_.GetProgress(), 0.0f, 1.0f);
	
		switch (mode_)
		{
		case Mode::FadeIn:
			alpha_ = 1.0f - progress;
			break;
		case Mode::FadeOut:
			alpha_ = progress;
			break;
		case Mode::None:
			alpha_ = 0.0f;
			break;
		}

		sprite_->SetColor(Vector4{0.0f, 0.0f, 0.0f, alpha_});
	}

}; // namespace KCE