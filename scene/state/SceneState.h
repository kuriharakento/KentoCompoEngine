#pragma once

namespace KCE
{
/**
 * @brief シーンの中の1つの段階（ステート）の基底クラス。
 * @tparam TOwner 持ち主のシーンの型。ステートはこの型のまま受け取るので、キャストは要らない
 *
 * - 2つ以上のステートで使うオブジェクトはシーンが持つ。ステートはシーンのアクセサ経由で触る
 * - そのステートだけが使う物（タイマー、演出のスプライトなど）はステートのメンバに持つ
 * - ステートはシーンが終わるまで消えない。入るたびにやり直す値は OnEnter で初期化する
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
	 * @brief シーンの Draw2D から毎フレーム呼ばれる。ステートが持つ 2D を描く。
	 * @param owner 持ち主のシーン
	 * @details 描くだけにする。状態を変えたり Request したりするのは OnUpdate で行う
	 */
	virtual void OnDraw2D(TOwner& owner) { (void)owner; }

	/**
	 * @brief このステートから出る瞬間に呼ばれる。
	 * @param owner 持ち主のシーン
	 */
	virtual void OnExit(TOwner& owner) { (void)owner; }
};
} // namespace KCE
