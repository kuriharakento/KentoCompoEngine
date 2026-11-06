#pragma once
#include "engine/gameobject/component/base/Behaviour.h"
#include "jsonEditor/JsonEditableBase.h"
#include "math/Vector3.h"

namespace KCE::GameObjectComponent
{
	/**
	 * @brief 一定の速さで回り続けるコンポーネント（ミラーボール、風車など）
	 * @details 持ち主の回転に毎フレーム足していくだけなので、止めて姿勢を戻せば元の角度に戻る
	 */
	class Rotator : public Behaviour, public JsonEditableBase
	{
	public:
		Rotator();
		~Rotator() override = default;

		void Update() override;

	private:
		// 軸ごとの回る速さ（度/秒）
		Vector3 degPerSec_{ 0.0f, 45.0f, 0.0f };
	};

	/**
	 * @brief 正弦波で行ったり来たりするコンポーネント（浮かぶ装飾、揺れる看板など）
	 * @details 持ち主の位置に、基準位置からのずれを足す。基準位置は持たず、前のフレームのずれとの差だけを足すので、
	 *          ギズモで動かした位置を基準にそのまま揺れる
	 */
	class Bobbing : public Behaviour, public JsonEditableBase
	{
	public:
		Bobbing();
		~Bobbing() override = default;

		void Update() override;
		/** @brief 止まっていた間の時間とずれを捨てる。再開で位置が跳ばないようにする */
		void OnUpdateResumed() override;

	private:
		// 軸ごとの振れ幅
		Vector3 amplitude_{ 0.0f, 0.3f, 0.0f };
		// 1秒あたりの往復回数
		float frequency_ = 0.5f;
		// 開始位置のずらし（1周を 1 とした割合）。同じ物を並べたときに動きをずらすのに使う
		float phase_ = 0.0f;

		// 更新で進んだ時間（保存しない）
		float time_ = 0.0f;
		// 今、基準位置に足してあるずれ（保存しない）
		Vector3 appliedOffset_{};
	};

	/**
	 * @brief このファイルのコンポーネントを確実にリンクさせる
	 * @details エンジンは静的ライブラリなので、どこからも参照されない .obj は取り込まれず、
	 *          REGISTER_COMPONENT の登録も走らない。Framework の初期化から呼んで取り込ませる
	 */
	void ForceLinkMotionComponents();
}
