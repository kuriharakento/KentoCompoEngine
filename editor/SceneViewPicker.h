#pragma once
#include <memory>

namespace KCE
{
class GameObject;
struct Matrix4x4;

/**
 * @brief Scene 画像をクリックして、その位置にある GameObject を選ぶ
 *
 * - 左クリック（ドラッグしていない）で、画像の位置からレイを飛ばし、描画物の境界箱に一番近く当たった GameObject を選ぶ
 * - 何にも当たらなければ選択を解除する
 * - ギズモの上を押したとき、Alt を押しているとき、Scene 画像の外から始まったクリックは無視する
 * - 描くのは DebugUIManager の Scene 重ね描きの中。ギズモのあとに登録して、ギズモの掴み判定を見られるようにする
 * - USE_IMGUI が無いときは何もしない
 */
class SceneViewPicker
{
public:
	static SceneViewPicker* GetInstance();
	static bool HasInstance();

	/** @brief DebugUIManager に Scene 重ね描きを登録する。SceneGizmo::Initialize の後に呼ぶ */
	void Initialize();
	/** @brief 登録を外して片付ける。DebugUIManager の終了より前に呼ぶ */
	void Finalize();

	~SceneViewPicker() = default;

private:
	static std::unique_ptr<SceneViewPicker> instance_;
	friend std::unique_ptr<SceneViewPicker> std::make_unique<SceneViewPicker>();

	SceneViewPicker() = default;
	SceneViewPicker(const SceneViewPicker&) = delete;
	SceneViewPicker& operator=(const SceneViewPicker&) = delete;

	/** @brief Scene 重ね描き。クリックを見て選択を変える */
	void Update();
	/**
	 * @brief 画面上の位置にある GameObject を探す
	 * @param viewProjection 今の Scene を描いているビュー射影行列
	 * @param ndcX 画像内の位置（左端 -1、右端 1）
	 * @param ndcY 画像内の位置（下端 -1、上端 1）
	 * @return 一番近く当たった GameObject。何にも当たらなければ nullptr
	 */
	GameObject* Pick(const Matrix4x4& viewProjection, float ndcX, float ndcY) const;

	// 押した位置が Scene 画像の上で、ギズモではなかったか。離したときにこれが真のときだけ選ぶ
	bool pressedOnScene_ = false;
};
} // namespace KCE
