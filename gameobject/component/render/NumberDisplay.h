#pragma once
#include <string>

#include "engine/gameobject/component/base/Behaviour.h"
#include "jsonEditor/JsonEditableBase.h"

namespace KCE
{
class GameObject;
namespace GameObjectComponent
{
/** @brief 静的ライブラリから、この .obj を確実に取り込ませるための空関数（REGISTER_COMPONENT を走らせる） */
void ForceLinkNumberDisplay();

/**
 * @brief 整数を、桁ごとの SpriteSheetComponent の板へ割り振るコンポーネント。親オブジェクトに付ける
 *
 * - 桁の板は、自分の直下の子から名前で探す（孫より下は見ない。入れ子の別の NumberDisplay の板と取り違えないため）。名前は digitPrefix + 桁番号（digit0 が 1 の位、digit1 が 10 の位…）
 * - 各桁の板の frame に、firstFrame + その桁の数字を入れる（numbers.png は 0 のコマが 0 番なので firstFrame = 0）
 * - hideLeadingZeros が真なら、上の桁が 0 の板を非表示にする。1 の位は常に出す
 * - 値が桁数に収まらないとき、clampOverflow が真なら全桁を 9 にし、偽なら下の桁だけ出す
 * - 値は SetValue で渡す。LateUpdate で、値が変わっていたときだけ板へ書く。マイナスは 0 にする
 * - 板の位置は、プレハブで決める（右揃えにするなら、1 の位を右端に置く）
 */
class NumberDisplay : public Behaviour, public JsonEditableBase
{
public:
	NumberDisplay();
	~NumberDisplay() override = default;

	void LateUpdate() override;

	/** @brief 表示する値。変わったときだけ、次の LateUpdate で板へ反映する */
	void SetValue(int value) { value_ = value; }
	int GetValue() const { return value_; }

#ifdef USE_IMGUI
	void DrawImGui() override;
#endif

private:
	/** @brief 値を板へ書く。1 つでも板が見つからなければ偽（次のフレームに、もう一度試す） */
	bool Apply();
	/** @brief 自分の直下の子から、名前の板を探す。無ければ nullptr */
	GameObject* FindDigit(int index) const;

	// --- プレハブに保存する値 ---
	int value_ = 0;
	std::string digitPrefix_ = "digit";
	int digitCount_ = 2;
	bool hideLeadingZeros_ = true;
	bool clampOverflow_ = true;
	int firstFrame_ = 0;

	// 最後に板へ書いた値と桁数（保存しない）。まだ書いていなければ -1
	int appliedValue_ = -1;
	int appliedCount_ = -1;
	bool appliedHide_ = false;
	// 板が足りなかったか（インスペクタの警告用）
	bool missingDigit_ = false;
};
} // namespace GameObjectComponent
} // namespace KCE
