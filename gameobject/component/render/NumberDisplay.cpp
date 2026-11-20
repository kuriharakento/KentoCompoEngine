#include "NumberDisplay.h"

#include <algorithm>

#include "engine/gameobject/base/GameObject.h"
#include "engine/gameobject/component/base/ComponentFactory.h"
#include "engine/gameobject/component/render/SpriteSheetComponent.h"

REGISTER_COMPONENT(NumberDisplay)

namespace KCE
{
namespace GameObjectComponent
{
namespace
{
/** @brief root の子孫から、名前の GameObject を探す（root 自身は含めない） */
GameObject* FindDescendant(const GameObject* root, const std::string& name)
{
	if (!root)
	{
		return nullptr;
	}
	for (const auto& child : root->GetChildren())
	{
		GameObject* object = child.second.get();
		if (!object)
		{
			continue;
		}
		if (object->GetName() == name)
		{
			return object;
		}
		if (GameObject* found = FindDescendant(object, name))
		{
			return found;
		}
	}
	return nullptr;
}

/** @brief 10 の n 乗。int に収まる範囲（n <= 9）だけ使う */
int Pow10(int n)
{
	int result = 1;
	for (int i = 0; i < n; ++i) { result *= 10; }
	return result;
}

constexpr int kMaxDigits = 9;
} // namespace

void ForceLinkNumberDisplay() {}

NumberDisplay::NumberDisplay()
{
	Register("value", &value_);
	Register("digitPrefix", &digitPrefix_);
	Register("digitCount", &digitCount_);
	Register("hideLeadingZeros", &hideLeadingZeros_);
	Register("clampOverflow", &clampOverflow_);
	Register("firstFrame", &firstFrame_);
}

GameObject* NumberDisplay::FindDigit(int index) const
{
	return FindDescendant(GetOwner(), digitPrefix_ + std::to_string(index));
}

bool NumberDisplay::Apply()
{
	const int count = std::clamp(digitCount_, 1, kMaxDigits);
	const int limit = Pow10(count);
	int value = (std::max)(value_, 0);
	value = clampOverflow_ ? (std::min)(value, limit - 1) : value % limit;

	bool allFound = true;
	for (int i = 0; i < count; ++i)
	{
		GameObject* digit = FindDigit(i);
		auto* sheet = digit ? digit->GetComponent<SpriteSheetComponent>() : nullptr;
		if (!sheet)
		{
			allFound = false;
			continue;
		}
		sheet->SetFrame(firstFrame_ + (value / Pow10(i)) % 10);
		// 上の桁が 0 のときは隠す。1 の位（i == 0）は、0 でも出す
		const bool show = i == 0 || !hideLeadingZeros_ || value >= Pow10(i);
		digit->SetVisible(show);
	}
	missingDigit_ = !allFound;
	return allFound;
}

void NumberDisplay::LateUpdate()
{
	// 値・桁数・隠す設定が変わっていないなら、何もしない。板が足りなかったときは、毎フレーム試す
	if (value_ == appliedValue_ && digitCount_ == appliedCount_ && hideLeadingZeros_ == appliedHide_ && !missingDigit_)
	{
		return;
	}
	if (Apply())
	{
		appliedValue_ = value_;
		appliedCount_ = digitCount_;
		appliedHide_ = hideLeadingZeros_;
	}
}

#ifdef USE_IMGUI
void NumberDisplay::DrawImGui()
{
	// 数値・文字・チェックの項目は、登録した値から出る
	JsonEditableBase::DrawImGui();

	if (missingDigit_)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "「%s0」〜「%s%d」の板が、子に見つからない（SpriteSheetComponent が要る）",
			digitPrefix_.c_str(), digitPrefix_.c_str(), (std::max)(digitCount_, 1) - 1);
	}
	if (digitCount_ > kMaxDigits)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "桁数は %d 桁まで", kMaxDigits);
	}
}
#endif
} // namespace GameObjectComponent
} // namespace KCE
