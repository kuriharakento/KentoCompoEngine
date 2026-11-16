#pragma once
#include <string>

#include "engine/gameobject/component/base/Behaviour.h"
#include "jsonEditor/JsonEditableBase.h"
#include "math/Vector3.h"

namespace KCE
{
class GameObject;
namespace GameObjectComponent
{
/** @brief 静的ライブラリから、この .obj を確実に取り込ませるための空関数（REGISTER_COMPONENT を走らせる） */
void ForceLinkRatioLink();

/**
 * @brief GaugeComponent の割合（0〜1）で、別のパラメータを動かすコンポーネント
 *
 * - 割合 0 のときの値（valueAt0）と 1 のときの値（valueAt1）を決めると、その間を線形に補間して対象へ書き込む
 * - ゲージ（source）: sourceName の GameObject（自分か、その子孫から名前で探す）。空なら自分。そこの GaugeComponent を使う
 * - 対象（target）: targetName の GameObject（同じく自分か子孫から探す）。空なら自分
 * - 対象の種類: Transform の位置・回転（度）・スケール、または対象のコンポーネントの登録パラメータ（float / Vector3）
 *   float のパラメータは valueAt0.x と valueAt1.x だけを使う
 * - 割合は追従後の見た目の値（GetDisplayRatio）を使う。useDisplayRatio を切ると目標の値（GetRatio）を使う
 * - 1 つのゲージに対して、これを複数付けてよい（針の回転と色など）
 * - 値は LateUpdate で書く。ゲージの Update が済んだ後の割合を使うため
 */
class RatioLink : public Behaviour, public JsonEditableBase
{
public:
	RatioLink();
	~RatioLink() override = default;

	void LateUpdate() override;

	/** @brief 対象の種類 */
	enum TargetKind
	{
		kPosition = 0,
		kRotationDeg = 1,
		kScale = 2,
		kComponentParam = 3,
	};

#ifdef USE_IMGUI
	void DrawImGui() override;
#endif

private:
	/** @brief 自分か子孫から、名前の GameObject を探す。空なら自分。無ければ nullptr */
	GameObject* Resolve(const std::string& name) const;

	std::string sourceName_;
	std::string targetName_;
	int targetKind_ = kRotationDeg;
	// kComponentParam のとき
	std::string componentType_;
	std::string paramKey_;
	Vector3 valueAt0_{ 0.0f, 0.0f, 0.0f };
	Vector3 valueAt1_{ 0.0f, 0.0f, 0.0f };
	bool useDisplayRatio_ = true;
	// 真なら割合を 1 - 割合 にする（割合が減るとき、値が 0 側から 1 側へ進むようにする）
	bool invertRatio_ = false;
};

/**
 * @brief GaugeComponent の割合を、時間で startRatio から endRatio へ動かすコンポーネント（タイマー・動作確認用）
 * @details ゲージの探し方は RatioLink の sourceName と同じ。loop が真なら、終わったら最初に戻る。
 *          止まっていた更新が再開したときは、経過時間を 0 に戻す
 */
class RatioTimer : public Behaviour, public JsonEditableBase
{
public:
	RatioTimer();
	~RatioTimer() override = default;

	void Update() override;
	void OnUpdateResumed() override { elapsed_ = 0.0f; }

private:
	std::string sourceName_;
	// 割合が動く秒数
	float duration_ = 10.0f;
	float startRatio_ = 1.0f;
	float endRatio_ = 0.0f;
	bool loop_ = true;

	// 経過時間（保存しない）
	float elapsed_ = 0.0f;
};
} // namespace GameObjectComponent
} // namespace KCE
