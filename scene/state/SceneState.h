#pragma once

namespace KCE
{
/**
 * @brief シーンの中の1つの段階（ステート）の基底クラス。
 * @tparam TOwner 持ち主のシーンの型。ステートはこの型のまま受け取るので、キャストは要らない
 *
 * - オブジェクトの所有はシーンのまま。ステートはシーンのアクセサ経由で触る
 * - ステートだけが使う値（タイマーなど）はステートのメンバに持つ
 * - 切り替えの判断は OnUpdate の中でして、StateMachine::Request で予約する
 */
template <typename TOwner>
class SceneState
{
public:
	virtual ~SceneState() = default;

	/**
	 * @brief このステートに入った瞬間に呼ばれる。
	 * @param owner 持ち主のシーン
	 */
	virtual void OnEnter(TOwner& owner) { (void)owner; }

	/**
	 * @brief 毎フレーム呼ばれる。
	 * @param owner 持ち主のシーン
	 */
	virtual void OnUpdate(TOwner& owner) { (void)owner; }

	/**
	 * @brief このステートから出る瞬間に呼ばれる。
	 * @param owner 持ち主のシーン
	 */
	virtual void OnExit(TOwner& owner) { (void)owner; }
};
} // namespace KCE
