#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

#include "math/Quaternion.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "sequencer/core/CurveTypes.h"

namespace KCE
{
/**
 * @brief 型ごとの補間方法を差し替えるためのトレイト
 * @details 既定は成分ごとの線形補間。Quaternion のみ Slerp に特殊化する。
 */
template<class T>
struct CurveInterpolator
{
	static T Interpolate(const T& a, const T& b, float t) { return a + (b - a) * t; }
};

/** @brief Quaternion は成分補間すると回転が縮むため Slerp を使う */
template<>
struct CurveInterpolator<Quaternion>
{
	static Quaternion Interpolate(const Quaternion& a, const Quaternion& b, float t)
	{
		return Quaternion::Slerp(a, b, t);
	}
};

/**
 * @brief カーブのキーフレーム
 * @details イージングは「このキーから次のキューへ向かう区間」に適用される。
 *          CSSの transition-timing-function と同じ考え方。
 */
template<class T>
struct Keyframe
{
	float time = 0.0f;
	T value{};
	InterpolationMode interp = InterpolationMode::Bezier;
	BezierHandle bezier{};
};

/**
 * @brief 任意時刻を評価できるカーブ
 * @details SEQUENCER_PLAN 3.1 の純関数契約に従い、内部状態を持たず
 *          Evaluate(time) が常に同じ値を返すことを保証する。
 *          キーは time の昇順に保たれる。
 */
template<class T>
class Curve
{
public:
	using KeyType = Keyframe<T>;

	/**
	 * @brief キーを追加し、時刻順を保つ
	 * @param key 追加するキーフレーム
	 * @return 追加後のキーのインデックス
	 */
	size_t AddKey(const KeyType& key)
	{
		const auto it = std::upper_bound(
			keys_.begin(), keys_.end(), key.time,
			[](float time, const KeyType& k) { return time < k.time; });
		const size_t index = static_cast<size_t>(it - keys_.begin());
		keys_.insert(it, key);
		return index;
	}

	/**
	 * @brief 時刻と値を指定してキーを追加する
	 * @param time 時刻（秒）
	 * @param value 値
	 * @return 追加後のキーのインデックス
	 */
	size_t AddKey(float time, const T& value)
	{
		KeyType key;
		key.time = time;
		key.value = value;
		return AddKey(key);
	}

	/**
	 * @brief キーを削除する
	 * @param index 削除するキーのインデックス
	 */
	void RemoveKey(size_t index)
	{
		if (index < keys_.size()) { keys_.erase(keys_.begin() + index); }
	}

	/**
	 * @brief キーの時刻を変更し、並び順を保つ
	 * @param index 対象キーのインデックス
	 * @param newTime 新しい時刻（秒）
	 * @return 移動後のキーのインデックス
	 */
	size_t MoveKey(size_t index, float newTime)
	{
		if (index >= keys_.size()) { return index; }
		KeyType key = keys_[index];
		key.time = newTime;
		keys_.erase(keys_.begin() + index);
		return AddKey(key);
	}

	/**
	 * @brief 指定時刻のカーブ値を評価する（純関数）
	 * @param time 評価する時刻（秒）
	 * @return 補間された値。キーが無い場合は既定値
	 */
	T Evaluate(float time) const
	{
		if (keys_.empty()) { return T{}; }
		if (keys_.size() == 1) { return keys_.front().value; }

		// 範囲外はクランプ（外挿はしない）
		if (time <= keys_.front().time) { return keys_.front().value; }
		if (time >= keys_.back().time) { return keys_.back().value; }

		const size_t index = FindSegment(time);
		const KeyType& a = keys_[index];
		const KeyType& b = keys_[index + 1];

		if (a.interp == InterpolationMode::Constant) { return a.value; }

		const float span = b.time - a.time;
		// 同時刻のキーが並んだ場合はゼロ除算を避けて次のキーへ飛ぶ
		if (span <= 0.0f) { return b.value; }

		float t = (time - a.time) / span;
		if (a.interp == InterpolationMode::Bezier) { t = ApplyBezierEasing(a.bezier, t); }

		return CurveInterpolator<T>::Interpolate(a.value, b.value, t);
	}

	/**
	 * @brief 指定時刻が属する区間の開始キーを二分探索する
	 * @param time 時刻（秒）
	 * @return 区間の開始キーのインデックス
	 */
	size_t FindSegment(float time) const
	{
		const auto it = std::upper_bound(
			keys_.begin(), keys_.end(), time,
			[](float t, const KeyType& k) { return t < k.time; });
		const size_t index = static_cast<size_t>(it - keys_.begin());
		return index == 0 ? 0 : index - 1;
	}

	/**
	 * @brief カーブが値を持つ最初の時刻
	 * @return 先頭キーの時刻。キーが無ければ0
	 */
	float GetStartTime() const { return keys_.empty() ? 0.0f : keys_.front().time; }

	/**
	 * @brief カーブが値を持つ最後の時刻
	 * @return 末尾キーの時刻。キーが無ければ0
	 */
	float GetEndTime() const { return keys_.empty() ? 0.0f : keys_.back().time; }

	bool IsEmpty() const { return keys_.empty(); }
	size_t GetKeyCount() const { return keys_.size(); }

	const std::vector<KeyType>& GetKeys() const { return keys_; }
	std::vector<KeyType>& GetKeys() { return keys_; }

	const KeyType& GetKey(size_t index) const { return keys_[index]; }
	KeyType& GetKey(size_t index) { return keys_[index]; }

	void Clear() { keys_.clear(); }

private:
	// 時刻の昇順に保たれたキー列
	std::vector<KeyType> keys_;
};

using FloatCurve = Curve<float>;
using Vector3Curve = Curve<Vector3>;
using Vector4Curve = Curve<Vector4>;
using QuaternionCurve = Curve<Quaternion>;
} // namespace KCE
