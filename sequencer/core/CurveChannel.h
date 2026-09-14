#pragma once
#include <string>

#include "sequencer/core/Curve.h"
#include "sequencer/core/CurveSerialization.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace KCE
{
/**
 * @brief トラックが持つカーブ1本を、値の型に関係なく扱うための窓口
 *
 * @details SEQUENCER_PLAN Phase 6 の「カーブ編集の共通化」。
 *          エディタはトラックの種類ごとに処理を書き分けず、
 *          この窓口だけを通してキーの選択・移動・削除・補間の編集を行う。
 *          トラックを1種類足すたびにエディタを書き換えずに済むようにするため。
 */
class ICurveChannel
{
public:
	virtual ~ICurveChannel() = default;

	/** @brief タイムラインに表示する名前 */
	virtual const char* GetName() const = 0;

	virtual size_t GetKeyCount() const = 0;
	virtual float GetKeyTime(size_t index) const = 0;

	/**
	 * @brief キーの時刻を変える
	 * @return 並べ替え後のキーのインデックス
	 */
	virtual size_t MoveKey(size_t index, float newTime) = 0;

	virtual void RemoveKey(size_t index) = 0;

	virtual InterpolationMode& GetKeyInterp(size_t index) = 0;
	virtual BezierHandle& GetKeyBezier(size_t index) = 0;

	/**
	 * @brief キーの間を補間するチャンネルか
	 * @details イベントのように「その時刻に起きるだけ」のキーは補間を持たない。
	 *          偽を返すチャンネルには、エディタは補間とベジェの編集UIを出さない。
	 */
	virtual bool HasInterpolation() const { return true; }

	/**
	 * @brief キー1つをコピー用の JSON にする
	 * @details 時刻は入れない。貼り付け先で決めるため。
	 * @param index キーのインデックス
	 * @return キーの中身。コピーに対応しないチャンネルなら null
	 */
	virtual nlohmann::json CopyKey(size_t index) const { (void)index; return nullptr; }

	/**
	 * @brief CopyKey() の JSON をキーとして貼り付ける
	 * @param time 貼り付ける時刻（秒）
	 * @param json CopyKey() が返した中身
	 * @return 貼り付けたら真。対応しない、または中身が読めなければ偽
	 */
	virtual bool PasteKey(float time, const nlohmann::json& json) { (void)time; (void)json; return false; }

	bool IsEmpty() const { return GetKeyCount() == 0; }

	/**
	 * @brief 指定時刻付近のキーを探す
	 * @return 見つかったインデックス。無ければ -1
	 */
	int FindKeyAt(float time, float tolerance) const
	{
		for (size_t i = 0; i < GetKeyCount(); ++i)
		{
			const float diff = GetKeyTime(i) - time;
			if (diff <= tolerance && diff >= -tolerance)
			{
				return static_cast<int>(i);
			}
		}
		return -1;
	}

#ifdef USE_IMGUI
	/**
	 * @brief キーの値を編集するUIを描く
	 * @return 値が変わったら真
	 */
	virtual bool DrawKeyValueEditor(size_t index) = 0;
#endif
};

namespace detail
{
#ifdef USE_IMGUI
/** @brief 値の型ごとの編集UI */
inline bool DrawCurveValue(const char* label, float& value) { return ImGui::DragFloat(label, &value, 0.01f); }
inline bool DrawCurveValue(const char* label, Vector3& value) { return ImGui::DragFloat3(label, &value.x, 0.05f); }
inline bool DrawCurveValue(const char* label, Vector4& value)
{
	// Vector4 のカーブは色に使う。HDR の強い色も打てるよう上限を外す
	return ImGui::ColorEdit4(label, &value.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
}
inline bool DrawCurveValue(const char* label, Quaternion& value)
{
	// 入力はオイラー角で受けるが、保持と補間はクォータニオンのまま行う
	Vector3 euler = value.ToEuler();
	if (ImGui::DragFloat3(label, &euler.x, 0.01f))
	{
		value = Quaternion::FromEuler(euler);
		return true;
	}
	return false;
}
#endif
} // namespace detail

/**
 * @brief Curve<T> を ICurveChannel として見せる薄い包み
 * @details カーブの実体はトラックが持つ。ここは参照するだけで所有しない。
 */
template<class T>
class CurveChannel : public ICurveChannel
{
public:
	CurveChannel(const char* name, Curve<T>* curve) : name_(name), curve_(curve) {}

	const char* GetName() const override { return name_; }
	size_t GetKeyCount() const override { return curve_->GetKeyCount(); }
	float GetKeyTime(size_t index) const override { return curve_->GetKey(index).time; }
	size_t MoveKey(size_t index, float newTime) override { return curve_->MoveKey(index, newTime); }
	void RemoveKey(size_t index) override { curve_->RemoveKey(index); }
	InterpolationMode& GetKeyInterp(size_t index) override { return curve_->GetKey(index).interp; }
	BezierHandle& GetKeyBezier(size_t index) override { return curve_->GetKey(index).bezier; }

	/**
	 * @brief 指定時刻にキーを打つ。同じ時刻に既存キーがあれば値を上書きする
	 * @details 浮動小数の誤差でキーが二重に増えないよう、1ミリ秒以内は同一とみなす。
	 */
	void SetKey(float time, const T& value)
	{
		const int existing = FindKeyAt(time, kSameTimeTolerance);
		if (existing >= 0)
		{
			curve_->GetKey(static_cast<size_t>(existing)).value = value;
			return;
		}
		curve_->AddKey(time, value);
	}

	nlohmann::json CopyKey(size_t index) const override
	{
		if (index >= curve_->GetKeyCount())
		{
			return nullptr;
		}
		const Keyframe<T>& key = curve_->GetKey(index);
		nlohmann::json json;
		json["value"] = CurveValueSerializer<T>::ToJson(key.value);
		json["interp"] = InterpolationModeToString(key.interp);
		json["bezier"] = nlohmann::json::array({ key.bezier.x1, key.bezier.y1, key.bezier.x2, key.bezier.y2 });
		return json;
	}

	/** @details 同じ時刻にキーがあれば、補間ごと上書きする */
	bool PasteKey(float time, const nlohmann::json& json) override
	{
		Keyframe<T> key;
		if (!json.is_object() || !json.contains("value") || !CurveValueSerializer<T>::FromJson(json["value"], key.value))
		{
			return false;
		}
		key.time = time;
		if (json.contains("interp") && json["interp"].is_string())
		{
			key.interp = InterpolationModeFromString(json["interp"].get<std::string>());
		}
		if (json.contains("bezier") && json["bezier"].is_array() && json["bezier"].size() == 4)
		{
			key.bezier = { json["bezier"][0].get<float>(), json["bezier"][1].get<float>(), json["bezier"][2].get<float>(), json["bezier"][3].get<float>() };
		}

		const int existing = FindKeyAt(time, kSameTimeTolerance);
		if (existing >= 0)
		{
			curve_->GetKey(static_cast<size_t>(existing)) = key;
			return true;
		}
		curve_->AddKey(key);
		return true;
	}

#ifdef USE_IMGUI
	bool DrawKeyValueEditor(size_t index) override
	{
		return detail::DrawCurveValue(name_, curve_->GetKey(index).value);
	}
#endif

private:
	// 浮動小数の誤差でキーが二重に増えないよう、1ミリ秒以内は同じ時刻とみなす
	static constexpr float kSameTimeTolerance = 0.001f;

	const char* name_;
	Curve<T>* curve_;
};
} // namespace KCE
