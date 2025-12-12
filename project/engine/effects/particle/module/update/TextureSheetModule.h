#pragma once
/**
 * @file TextureSheetModule.h
 * @brief テクスチャシートアニメーションモジュール
 * 
 * スプライトシートのフリップブックアニメーション制御。
 * Loop, Once, PingPong等の再生モードに対応。
 */
#include "effects/particle/module/IModule.h"
#include "effects/particle/module/ModulePriorities.h"
#include "effects/particle/ParticleEmitter.h"
#include "effects/particle/ParticleTypes.h"
#include <algorithm>

/**
 * @brief テクスチャシートアニメーションモジュール
 * 
 * スプライトシート（テクスチャアトラス）のフレームアニメーションを制御する。
 * 爆発やエネルギー系エフェクトのフリップブックアニメーションに使用。
 */
class TextureSheetModule : public IModule
{
public:
	static constexpr uint32_t kDefaultColumns = 4;
	static constexpr uint32_t kDefaultRows = 4;
	static constexpr float kDefaultFrameRate = 30.0f;
	
	/**
	 * @brief コンストラクタ
	 * @param columns グリッド列数
	 * @param rows グリッド行数
	 * @param fps フレームレート
	 */
	TextureSheetModule(uint32_t columns = kDefaultColumns, uint32_t rows = kDefaultRows, float fps = kDefaultFrameRate)
		: columns_(columns), rows_(rows), frameRate_(fps)
	{
		totalFrames_ = columns_ * rows_;
	}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (!particle.IsAlive()) continue;

			float frameTime = particle.age * frameRate_;
			uint32_t frameIndex = static_cast<uint32_t>(frameTime);

			switch (playMode_)
			{
			case TextureSheetPlayMode::Loop:
				particle.spriteIndex = frameIndex % totalFrames_;
				break;

			case TextureSheetPlayMode::Once:
				particle.spriteIndex = (std::min)(frameIndex, totalFrames_ - 1);
				break;

			case TextureSheetPlayMode::PingPong:
			{
				uint32_t cycle = frameIndex / totalFrames_;
				uint32_t localFrame = frameIndex % totalFrames_;
				if (cycle % 2 == 1)
				{
					// 逆再生
					particle.spriteIndex = totalFrames_ - 1 - localFrame;
				}
				else
				{
					particle.spriteIndex = localFrame;
				}
				break;
			}
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "TextureSheet"; }
	int32_t GetPriority() const override { return 80; }

	/**
	 * @brief グリッドサイズを設定
	 * @param columns 列数
	 * @param rows 行数
	 */
	void SetGridSize(uint32_t columns, uint32_t rows)
	{
		columns_ = columns;
		rows_ = rows;
		totalFrames_ = columns_ * rows_;
	}

	/**
	 * @brief フレームレートを設定
	 * @param fps フレームレート
	 */
	void SetFrameRate(float fps) { frameRate_ = fps; }
	
	/**
	 * @brief 再生モードを設定
	 * @param mode 再生モード
	 */
	void SetPlayMode(TextureSheetPlayMode mode) { playMode_ = mode; }
	
	/**
	 * @brief 開始フレームを設定
	 * @param frame 開始フレーム番号
	 */
	void SetStartFrame(uint32_t frame) { startFrame_ = frame; }

	/**
	 * @brief グリッド列数を取得
	 * @return 列数
	 */
	uint32_t GetColumns() const { return columns_; }
	
	/**
	 * @brief グリッド行数を取得
	 * @return 行数
	 */
	uint32_t GetRows() const { return rows_; }
	
	/**
	 * @brief 総フレーム数を取得
	 * @return 総フレーム数
	 */
	uint32_t GetTotalFrames() const { return totalFrames_; }
	
	/**
	 * @brief フレームレートを取得
	 * @return フレームレート
	 */
	float GetFrameRate() const { return frameRate_; }
	
	/**
	 * @brief 再生モードを取得
	 * @return 再生モード
	 */
	TextureSheetPlayMode GetPlayMode() const { return playMode_; }

private:
	uint32_t columns_ = kDefaultColumns;
	uint32_t rows_ = kDefaultRows;
	uint32_t totalFrames_ = kDefaultColumns * kDefaultRows;
	float frameRate_ = kDefaultFrameRate;
	uint32_t startFrame_ = 0;
	TextureSheetPlayMode playMode_ = TextureSheetPlayMode::Loop;
};
