#pragma once
#include <cstdint>
#include <memory>

#include "graphics/2d/Sprite.h"
#include "time/Timer.h"

namespace KCE
{
	class SpriteCommon;

	class Fade
	{
	public:
		Fade() = default;
		~Fade() = default;

		void Initialize(SpriteCommon* spriteCommon);

		void Update();

		void Draw();

		void StartFadeIn(float duration);

		void StartFadeOut(float duration);

	public: // 　アクセッサ
		bool IsFading() const { return mode_ != Mode::None; }

		bool IsFinished() const { return mode_ == Mode::None || timer_.IsFinished(); }
	private:
		enum class Mode : uint8_t
		{
			None,
			FadeIn,
			FadeOut
		};

	private:
		void ApplyAlpha();

		void Begin(Mode mode, float duration);

		std::unique_ptr<Sprite> sprite_;
		Mode mode_ = Mode::None;
		Timer timer_;
		float alpha_ = 0.0f;
	};

} // namespace KCE