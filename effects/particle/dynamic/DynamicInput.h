#pragma once
/**
 * @file DynamicInput.h
 * @brief 動的入力システム
 * 
 * アニメーションカーブ、カラーグラデーション、ランダム範囲など
 * 時間やランダム性に基づく動的な値入力を提供。
 */
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "effects/particle/module/ModuleRuntime.h"
#include <vector>
#include <cstdlib>
#include <algorithm>

namespace KCE
{
inline float DynamicInputRandom01(uint32_t seed)
{
	seed ^= seed >> 16; seed *= 0x7feb352du; seed ^= seed >> 15; seed *= 0x846ca68bu; seed ^= seed >> 16;
	return static_cast<float>(seed & 0x00ffffffu) / static_cast<float>(0x01000000u);
}
/**
 * @brief アニメーションカーブ（キーフレーム補間）
 */
struct AnimationCurve
{
	struct Key
	{
		float time;   // 0.0 ~ 1.0
		float value;
	};

	std::vector<Key> keys;

	AnimationCurve() = default;
	AnimationCurve(float constantValue)
	{
		keys.push_back({ 0.0f, constantValue });
		keys.push_back({ 1.0f, constantValue });
	}

	float Evaluate(float t) const
	{
		if (keys.empty()) return 0.0f;
		if (keys.size() == 1) return keys[0].value;

		t = std::clamp(t, 0.0f, 1.0f);

		// 最初と最後のキーより外
		if (t <= keys.front().time) return keys.front().value;
		if (t >= keys.back().time) return keys.back().value;

		// 線形補間
		for (size_t i = 0; i < keys.size() - 1; ++i)
		{
			if (t >= keys[i].time && t <= keys[i + 1].time)
			{
				const float span = keys[i + 1].time - keys[i].time;
				float localT = span > 0.0f ? (t - keys[i].time) / span : 0.0f;
				return keys[i].value + (keys[i + 1].value - keys[i].value) * localT;
			}
		}

		return keys.back().value;
	}

	void AddKey(float time, float value)
	{
		time = std::clamp(time, 0.0f, 1.0f);
		for (auto& key : keys) if (key.time == time) { key.value = value; return; }
		keys.push_back({ time, value });
		std::sort(keys.begin(), keys.end(), [](const Key& a, const Key& b) {
			return a.time < b.time;
		});
	}
};

/**
 * @brief カラーグラデーション
 */
struct ColorGradient
{
	struct Key
	{
		float time;   // 0.0 ~ 1.0
		Vector4 color;
	};

	std::vector<Key> keys;

	ColorGradient() = default;
	ColorGradient(const Vector4& constantColor)
	{
		keys.push_back({ 0.0f, constantColor });
		keys.push_back({ 1.0f, constantColor });
	}

	Vector4 Evaluate(float t) const
	{
		if (keys.empty()) return { 1, 1, 1, 1 };
		if (keys.size() == 1) return keys[0].color;

		t = std::clamp(t, 0.0f, 1.0f);

		if (t <= keys.front().time) return keys.front().color;
		if (t >= keys.back().time) return keys.back().color;

		for (size_t i = 0; i < keys.size() - 1; ++i)
		{
			if (t >= keys[i].time && t <= keys[i + 1].time)
			{
				const float span = keys[i + 1].time - keys[i].time;
				float localT = span > 0.0f ? (t - keys[i].time) / span : 0.0f;
				Vector4 result;
				result.x = keys[i].color.x + (keys[i + 1].color.x - keys[i].color.x) * localT;
				result.y = keys[i].color.y + (keys[i + 1].color.y - keys[i].color.y) * localT;
				result.z = keys[i].color.z + (keys[i + 1].color.z - keys[i].color.z) * localT;
				result.w = keys[i].color.w + (keys[i + 1].color.w - keys[i].color.w) * localT;
				return result;
			}
		}

		return keys.back().color;
	}

	void AddKey(float time, const Vector4& color)
	{
		time = std::clamp(time, 0.0f, 1.0f);
		for (auto& key : keys) if (key.time == time) { key.color = color; return; }
		keys.push_back({ time, color });
		std::sort(keys.begin(), keys.end(), [](const Key& a, const Key& b) {
			return a.time < b.time;
		});
	}
};

/**
 * @brief 動的入力モード
 */
enum class DynamicInputMode
{
	Constant,     // 固定値
	RandomRange,  // ランダム範囲
	Curve         // カーブ/グラデーション
	,EmitterParameter // emitter/user parameter namespace binding
};

/**
 * @brief 動的Float入力
 */
class DynamicFloat
{
public:
	DynamicFloat() = default;
	DynamicFloat(float constant) : constant_(constant), mode_(DynamicInputMode::Constant) {}

	float Evaluate(float t = 0.0f, const ModuleParameterStore* parameters = nullptr, uint32_t seed = 0) const
	{
		switch (mode_)
		{
		case DynamicInputMode::Constant:
			return constant_;
		case DynamicInputMode::RandomRange:
			return min_ + DynamicInputRandom01(seed) * (max_ - min_);
		case DynamicInputMode::Curve:
			return curve_.Evaluate(t);
		case DynamicInputMode::EmitterParameter:
			if (parameters) if (const float* value = parameters->FindAs<float>(parameterName_)) return *value;
			return constant_;
		}
		return constant_;
	}

	void SetConstant(float value)
	{
		mode_ = DynamicInputMode::Constant;
		constant_ = value;
	}

	void SetRandomRange(float min, float max)
	{
		mode_ = DynamicInputMode::RandomRange;
		min_ = min;
		max_ = max;
	}

	void SetCurve(const AnimationCurve& curve)
	{
		mode_ = DynamicInputMode::Curve;
		curve_ = curve;
	}
	void SetEmitterParameter(std::string name, float fallback = 0.0f)
	{
		mode_ = DynamicInputMode::EmitterParameter; parameterName_ = std::move(name); constant_ = fallback;
	}
	const std::string& GetEmitterParameter() const { return parameterName_; }

	DynamicInputMode GetMode() const { return mode_; }
	float GetConstant() const { return constant_; }
	float GetMin() const { return min_; }
	float GetMax() const { return max_; }
	const AnimationCurve& GetCurve() const { return curve_; }

private:
	DynamicInputMode mode_ = DynamicInputMode::Constant;
	float constant_ = 0.0f;
	float min_ = 0.0f;
	float max_ = 1.0f;
	AnimationCurve curve_;
	std::string parameterName_;
};

/**
 * @brief 動的Vector3入力
 */
class DynamicVector3
{
public:
	DynamicVector3() = default;
	DynamicVector3(const Vector3& constant) : constant_(constant), mode_(DynamicInputMode::Constant) {}

	Vector3 Evaluate(float t = 0.0f, const ModuleParameterStore* parameters = nullptr, uint32_t seed = 0) const
	{
		switch (mode_)
		{
		case DynamicInputMode::Constant:
			return constant_;
		case DynamicInputMode::RandomRange:
		{
			Vector3 result;
			result.x = min_.x + DynamicInputRandom01(seed) * (max_.x - min_.x);
			result.y = min_.y + DynamicInputRandom01(seed ^ 0x9e3779b9u) * (max_.y - min_.y);
			result.z = min_.z + DynamicInputRandom01(seed ^ 0x85ebca6bu) * (max_.z - min_.z);
			return result;
		}
		case DynamicInputMode::Curve:
		{
			Vector3 result;
			result.x = curveX_.Evaluate(t);
			result.y = curveY_.Evaluate(t);
			result.z = curveZ_.Evaluate(t);
			return result;
		}
		case DynamicInputMode::EmitterParameter:
			if (parameters) if (const Vector3* value = parameters->FindAs<Vector3>(parameterName_)) return *value;
			return constant_;
		}
		return constant_;
	}

	void SetConstant(const Vector3& value)
	{
		mode_ = DynamicInputMode::Constant;
		constant_ = value;
	}

	void SetRandomRange(const Vector3& min, const Vector3& max)
	{
		mode_ = DynamicInputMode::RandomRange;
		min_ = min;
		max_ = max;
	}

	void SetCurves(const AnimationCurve& x, const AnimationCurve& y, const AnimationCurve& z)
	{
		mode_ = DynamicInputMode::Curve;
		curveX_ = x;
		curveY_ = y;
		curveZ_ = z;
	}
	void SetEmitterParameter(std::string name, const Vector3& fallback = {})
	{
		mode_ = DynamicInputMode::EmitterParameter; parameterName_ = std::move(name); constant_ = fallback;
	}

	DynamicInputMode GetMode() const { return mode_; }
	const Vector3& GetConstant() const { return constant_; }
	const Vector3& GetMin() const { return min_; }
	const Vector3& GetMax() const { return max_; }
	const AnimationCurve& GetCurveX() const { return curveX_; }
	const AnimationCurve& GetCurveY() const { return curveY_; }
	const AnimationCurve& GetCurveZ() const { return curveZ_; }
	const std::string& GetEmitterParameter() const { return parameterName_; }

private:
	DynamicInputMode mode_ = DynamicInputMode::Constant;
	Vector3 constant_ = {};
	Vector3 min_ = {};
	Vector3 max_ = { 1, 1, 1 };
	AnimationCurve curveX_;
	AnimationCurve curveY_;
	AnimationCurve curveZ_;
	std::string parameterName_;
};

/**
 * @brief 動的Color入力
 */
class DynamicColor
{
public:
	DynamicColor() = default;
	DynamicColor(const Vector4& constant) : constant_(constant), mode_(DynamicInputMode::Constant) {}

	Vector4 Evaluate(float t = 0.0f, const ModuleParameterStore* parameters = nullptr, uint32_t seed = 0) const
	{
		switch (mode_)
		{
		case DynamicInputMode::Constant:
			return constant_;
		case DynamicInputMode::RandomRange:
		{
			Vector4 result;
			result.x = min_.x + DynamicInputRandom01(seed) * (max_.x - min_.x);
			result.y = min_.y + DynamicInputRandom01(seed ^ 0x9e3779b9u) * (max_.y - min_.y);
			result.z = min_.z + DynamicInputRandom01(seed ^ 0x85ebca6bu) * (max_.z - min_.z);
			result.w = min_.w + DynamicInputRandom01(seed ^ 0xc2b2ae35u) * (max_.w - min_.w);
			return result;
		}
		case DynamicInputMode::Curve:
			return gradient_.Evaluate(t);
		case DynamicInputMode::EmitterParameter:
			if (parameters) if (const Vector4* value = parameters->FindAs<Vector4>(parameterName_)) return *value;
			return constant_;
		}
		return constant_;
	}

	void SetConstant(const Vector4& color)
	{
		mode_ = DynamicInputMode::Constant;
		constant_ = color;
	}

	void SetRandomRange(const Vector4& min, const Vector4& max)
	{
		mode_ = DynamicInputMode::RandomRange;
		min_ = min;
		max_ = max;
	}

	void SetGradient(const ColorGradient& gradient)
	{
		mode_ = DynamicInputMode::Curve;
		gradient_ = gradient;
	}
	void SetEmitterParameter(std::string name, const Vector4& fallback = { 1, 1, 1, 1 })
	{
		mode_ = DynamicInputMode::EmitterParameter; parameterName_ = std::move(name); constant_ = fallback;
	}

	DynamicInputMode GetMode() const { return mode_; }
	const Vector4& GetConstant() const { return constant_; }
	const Vector4& GetMin() const { return min_; }
	const Vector4& GetMax() const { return max_; }
	const ColorGradient& GetGradient() const { return gradient_; }
	const std::string& GetEmitterParameter() const { return parameterName_; }

private:
	DynamicInputMode mode_ = DynamicInputMode::Constant;
	Vector4 constant_ = { 1, 1, 1, 1 };
	Vector4 min_ = {};
	Vector4 max_ = { 1, 1, 1, 1 };
	ColorGradient gradient_;
	std::string parameterName_;
};
} // namespace KCE
